#include "backend.hpp"

#ifdef _WIN32
#include <SDL_video.h>
#else
#include <SDL2/SDL_video.h>
#endif
#include <SDL_events.h>

#include "plume_vulkan.h"
#include "common/rt64_user_configuration.h"

#include "concurrentqueue.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_vulkan.h"

#if defined(_WIN32)
#   include "imgui/backends/imgui_impl_dx12.h"
#   include "imgui/backends/imgui_impl_win32.h"
#   include "utf8conv/utf8conv.h"
#endif

#if defined(__APPLE__)
#   include "imgui/backends/imgui_impl_metal.h"
#endif

#if defined(_WIN32)
#include "plume_d3d12.h"
#endif

#include "renderer/hooks.hpp"
#include "config/config.hpp"
#include "runtime/gfx.hpp"

namespace dino::debug_ui::backend {

struct VulkanContext {
    VkDevice device = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    ~VulkanContext() {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
        }

        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }
    }
};

// We must use our own ImGui context to avoid conflicting with the RT64 inspector where possible
ImGuiContext *dino_imgui_ctx;
bool b_is_open = false;
bool b_in_ui_frame = false;

static ImGuiContext *prev_ctx;
static std::unique_ptr<plume::RenderDescriptorSet> descriptor_set;
static std::mutex frame_mutex;
static moodycamel::ConcurrentQueue<SDL_Event> event_queue{};
static std::unique_ptr<VulkanContext> vulkanContext;

static RT64::UserConfiguration::GraphicsAPI get_graphics_api() {
    const ultramodern::renderer::GraphicsConfig &config = ultramodern::renderer::get_graphics_config();

    switch (config.api_option) {
        case ultramodern::renderer::GraphicsApi::D3D12:
            return RT64::UserConfiguration::GraphicsAPI::D3D12;
        case ultramodern::renderer::GraphicsApi::Vulkan:
            return RT64::UserConfiguration::GraphicsAPI::Vulkan;
        case ultramodern::renderer::GraphicsApi::Auto: {
#if defined(_WIN32)
            return RT64::UserConfiguration::GraphicsAPI::D3D12;
#elif defined(__gnu_linux__)
            return RT64::UserConfiguration::GraphicsAPI::Vulkan;
#elif defined(__APPLE__)
            return RT64::UserConfiguration::GraphicsAPI::Metal;
#else
            static_assert(false && "Unimplemented")
#endif
        }
        default:
            assert(false && "Unknown Graphics API.");
            return RT64::UserConfiguration::GraphicsAPI::OptionCount;
    }
}

bool is_open() { return b_is_open; }
bool in_ui_frame() { return b_in_ui_frame; }

void begin() {
    if (!dino_imgui_ctx->Initialized) return;

    if (!b_is_open) {
        // Still process the event queue but don't send any to ImGui
        SDL_Event event{};
        while (event_queue.try_dequeue(event)) {
            if (dino::config::get_debug_ui_enabled()) {
                if (event.type == SDL_KEYDOWN && 
                        (event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_GRAVE || 
                            event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_F9) &&
                        !event.key.repeat) {
                    b_is_open = !b_is_open;
                }
            }
        }
    } else if (!dino::config::get_debug_ui_enabled()) {
        b_is_open = false;
    }

    if (!b_is_open) {
        // Still not open even after handling key events
        return;
    }

    assert(!b_in_ui_frame);
    b_in_ui_frame = true;

    frame_mutex.lock();

    prev_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(dino_imgui_ctx);

    SDL_Event event{};
    while (event_queue.try_dequeue(event)) {
        if (event.type == SDL_KEYDOWN && 
                (event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_GRAVE || 
                    event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_F9) &&
                !event.key.repeat) {
            b_is_open = !b_is_open;
        }

        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    if (!b_is_open) {
        // Closed after handling key events
        b_in_ui_frame = false;
        ImGui::SetCurrentContext(prev_ctx);
        frame_mutex.unlock();
        return;
    }
    
    ImGui_ImplSDL2_NewFrame();

    switch (get_graphics_api()) {
        case RT64::UserConfiguration::GraphicsAPI::D3D12: {
#ifdef _WIN32
            ImGui_ImplDX12_NewFrame();
#else
            assert(false && "Unsupported Graphics API.");
            return;
#endif
            break;
        }
        case RT64::UserConfiguration::GraphicsAPI::Vulkan: {
            ImGui_ImplVulkan_NewFrame();
            break;
        }
        case RT64::UserConfiguration::GraphicsAPI::Metal: {
#ifdef __APPLE__
            ImGui_ImplMetal_NewFrame(nullptr);
#endif
            break;
        }
        default:
            assert(false && "Unknown Graphics API.");
            break;
    }

    ImGui::NewFrame();
}

void end() {
    if (b_in_ui_frame) {
        assert(dino_imgui_ctx->Initialized);
        
        ImGui::Render();
        ImGui::SetCurrentContext(prev_ctx);

        frame_mutex.unlock();

        b_in_ui_frame = false;
    }
}

static int sdl_event_filter(void *userdata, SDL_Event *event) {
    event_queue.enqueue(*event);

    return 1;
}

static void checkVulkanResult(VkResult res) {
    if (res == 0) {
        return;
    }

    fprintf(stderr, "Debug UI ImGui Vulkan Backend failed with error code 0x%X.\n", res);
}

// TODO: why isn't this available in imgui_impl_vulkan.h? do we have an outdated version?
static uint32_t ImGui_ImplVulkanH_SelectQueueFamilyIndex(VkPhysicalDevice physical_device)
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
    ImVector<VkQueueFamilyProperties> queues_properties;
    queues_properties.resize((int)count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queues_properties.Data);
    for (uint32_t i = 0; i < count; i++)
        if (queues_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            return i;
    return (uint32_t)-1;
}

static void rt64_init_hook(plume::RenderInterface* _interface, plume::RenderDevice* device) {
    IMGUI_CHECKVERSION();
    
    ImGuiContext *prev_ctx = ImGui::GetCurrentContext();

    dino_imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(dino_imgui_ctx);

    static std::string imgui_ini_path = (dino::config::get_app_folder_path() / "imgui.ini").string();
    dino_imgui_ctx->IO.IniFilename = imgui_ini_path.c_str();

    ImGui_ImplSDL2_InitForOther(dino::runtime::get_window());

    switch (get_graphics_api()) {
        case RT64::UserConfiguration::GraphicsAPI::D3D12: {
#ifdef _WIN32
            plume::D3D12Device *interface_device = static_cast<plume::D3D12Device *>(device);
            plume::RenderDescriptorRange descriptor_range(plume::RenderDescriptorRangeType::TEXTURE, 0, 1);
            descriptor_set = interface_device->createDescriptorSet(plume::RenderDescriptorSetDesc(&descriptor_range, 1));

            plume::D3D12DescriptorSet *interface_descriptor_set = static_cast<plume::D3D12DescriptorSet *>(descriptor_set.get());
            const D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = interface_device->viewHeapAllocator->getCPUHandleAt(interface_descriptor_set->viewAllocation.offset);
            const D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = interface_device->viewHeapAllocator->getGPUHandleAt(interface_descriptor_set->viewAllocation.offset);
            ImGui_ImplDX12_Init(interface_device->d3d, 2, DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM, interface_device->viewHeapAllocator->heap, cpu_handle, gpu_handle);
#else
            assert(false && "Unsupported Graphics API.");
            return;
#endif
            break;
        }
        case RT64::UserConfiguration::GraphicsAPI::Vulkan: {
            plume::VulkanDevice *interfaceDevice = static_cast<plume::VulkanDevice *>(device);
            ImGui_ImplVulkan_LoadFunctions([](const char *functionName, void *vulkanInstance) {
                return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance *>(vulkanInstance)), functionName);
            }, &interfaceDevice->renderInterface->instance);

            std::unordered_map<VkDescriptorType, uint32_t> typeCounts;
            typeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 1;

            const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM/*, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM*/ };
            
            vulkanContext = std::make_unique<VulkanContext>();
            vulkanContext->device = interfaceDevice->vk;
            vulkanContext->renderPass = plume::VulkanGraphicsPipeline::createRenderPass(interfaceDevice, requestSurfaceImageFormat, 
                (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT);
            vulkanContext->descriptorPool = plume::VulkanDescriptorSet::createDescriptorPool(interfaceDevice, typeCounts, false);

            uint32_t queueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(interfaceDevice->physicalDevice);
            IM_ASSERT(queueFamily != (uint32_t)-1);

            VkQueue queue;
            vkGetDeviceQueue(interfaceDevice->vk, queueFamily, 0, &queue);

            ImGui_ImplVulkan_InitInfo initInfo = {};
            initInfo.Instance = interfaceDevice->renderInterface->instance;
            initInfo.PhysicalDevice = interfaceDevice->physicalDevice;
            initInfo.Device = vulkanContext->device;
            initInfo.QueueFamily = queueFamily;
            initInfo.Queue = queue;
            initInfo.DescriptorPool = vulkanContext->descriptorPool;
            initInfo.RenderPass = vulkanContext->renderPass;
            initInfo.Subpass = 0;
            initInfo.MinImageCount = 2;
            initInfo.ImageCount = 2;
            initInfo.CheckVkResultFn = &checkVulkanResult;

            ImGui_ImplVulkan_Init(&initInfo);
            break;
        }
        case RT64::UserConfiguration::GraphicsAPI::Metal: {
#ifdef __APPLE__
            ImGui_ImplMetal_Init(nullptr);
#else
            assert(false && "Metal only available on macOS.");
            return;
#endif
            break;
        }
        default:
            assert(false && "Unknown Graphics API.");
            break;
    }

    SDL_AddEventWatch(sdl_event_filter, nullptr);

    ImGui::SetCurrentContext(prev_ctx);
}

static void rt64_draw_hook(plume::RenderCommandList* command_list, plume::RenderFramebuffer* swap_chain_framebuffer) {
    if (!b_is_open) return;
    
    const std::lock_guard<std::mutex> frame_lock(frame_mutex);

    ImGuiContext *prev_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(dino_imgui_ctx);

    ImDrawData *draw_data = ImGui::GetDrawData();
    if (draw_data != nullptr) {
        switch (get_graphics_api()) {
            case RT64::UserConfiguration::GraphicsAPI::D3D12: {
#ifdef _WIN32
                plume::D3D12CommandList *interface_command_list = static_cast<plume::D3D12CommandList *>(command_list);
                interface_command_list->checkDescriptorHeaps();
                ImGui_ImplDX12_RenderDrawData(draw_data, interface_command_list->d3d);
#else
                assert(false && "Unsupported Graphics API.");
#endif
                break;
            }
            case RT64::UserConfiguration::GraphicsAPI::Vulkan: {
                plume::VulkanCommandList *interface_command_list = static_cast<plume::VulkanCommandList *>(command_list);
                ImGui_ImplVulkan_RenderDrawData(draw_data, interface_command_list->vk);
                break;
            }
            case RT64::UserConfiguration::GraphicsAPI::Metal: {
#ifdef __APPLE__
                ImGui_ImplMetal_RenderDrawData(draw_data, nullptr);
#endif
                break;
            }
            default:
                assert(false && "Unknown Graphics API.");
                break;
        }
    }

    ImGui::SetCurrentContext(prev_ctx);
}

static void rt64_deinit_hook() {
    const std::lock_guard<std::mutex> frame_lock(frame_mutex);
    assert(!b_in_ui_frame);
    b_is_open = false;

    ImGuiContext *prev_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(dino_imgui_ctx);

    switch (get_graphics_api()) {
        case RT64::UserConfiguration::GraphicsAPI::D3D12:
#ifdef _WIN32
            ImGui_ImplDX12_Shutdown();
            descriptor_set.release();
#else
            assert(false && "Unsupported Graphics API.");
            return;
#endif
            break;
        case RT64::UserConfiguration::GraphicsAPI::Vulkan:
            ImGui_ImplVulkan_Shutdown();
            vulkanContext.reset(nullptr);
            break;
        case RT64::UserConfiguration::GraphicsAPI::Metal: {
#ifdef __APPLE__
            ImGui_ImplMetal_Shutdown();
#endif
            break;
        }
        default:
            assert(false && "Unknown Graphics API.");
            break;
    }

    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext(dino_imgui_ctx);
    ImGui::SetCurrentContext(prev_ctx);
}

void set_render_hooks() {
    dino::renderer::add_hook(rt64_init_hook, rt64_draw_hook, rt64_deinit_hook);
}

}
