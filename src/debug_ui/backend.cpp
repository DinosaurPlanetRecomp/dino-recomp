#include "backend.hpp"

#ifdef _WIN32
#include <SDL_video.h>
#else
#include <SDL2/SDL_video.h>
#endif
#include <SDL_events.h>

#include "plume_vulkan.h"
#include "common/rt64_user_configuration.h"
#include "hle/rt64_application.h"

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

    ~VulkanContext() {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
        }
    }
};

// We must use our own ImGui context to avoid conflicting with the RT64 inspector where possible
ImGuiContext *dino_imgui_ctx;
bool b_is_open = false;
bool b_in_ui_frame = false;

static ImGuiContext *prev_ctx;
static std::unique_ptr<plume::RenderDescriptorSet> descriptorSet;
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
            // TODO: Add MoltenVK option for Mac?
            return RT64::UserConfiguration::GraphicsAPI::Vulkan;
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
            if (event.type == SDL_KEYDOWN && 
                    (event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_GRAVE || 
                        event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_F9) &&
                    !event.key.repeat) {
                b_is_open = !b_is_open;
            }
        }
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

void init(RT64::Application* app) {
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
            D3D12Device *interfaceDevice = static_cast<D3D12Device *>(app->device.get());
            RenderDescriptorRange descriptorRange(RenderDescriptorRangeType::TEXTURE, 0, 1);
            descriptorSet = interfaceDevice->createDescriptorSet(RenderDescriptorSetDesc(&descriptorRange, 1));

            D3D12DescriptorSet *interfaceDescriptorSet = static_cast<D3D12DescriptorSet *>(descriptorSet.get());
            const D3D12SwapChain *interfaceSwapChain = static_cast<const D3D12SwapChain *>(app->swapChain.get());
            const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = interfaceDevice->viewHeapAllocator->getCPUHandleAt(interfaceDescriptorSet->viewAllocation.offset);
            const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = interfaceDevice->viewHeapAllocator->getGPUHandleAt(interfaceDescriptorSet->viewAllocation.offset);
            ImGui_ImplDX12_InitInfo initInfo;
            initInfo.Device = interfaceDevice->d3d;
            initInfo.CommandQueue = interfaceSwapChain->commandQueue->d3d;
            initInfo.NumFramesInFlight = 2;
            initInfo.RTVFormat = interfaceSwapChain->nativeFormat;
            initInfo.SrvDescriptorHeap = interfaceDevice->viewHeapAllocator->heap;
            initInfo.LegacySingleSrvCpuDescriptor = cpuHandle;
            initInfo.LegacySingleSrvGpuDescriptor = gpuHandle;

            ImGui_ImplDX12_Init(&initInfo);
#else
            assert(false && "Unsupported Graphics API.");
            return;
#endif
            break;
        }
        case RT64::UserConfiguration::GraphicsAPI::Vulkan: {
            VulkanDevice *interfaceDevice = static_cast<VulkanDevice *>(app->device.get());
            const VulkanSwapChain *interfaceSwapChain = static_cast<const VulkanSwapChain *>(app->swapChain.get());
            ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_2, [](const char *functionName, void *vulkanInstance) {
                return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance *>(vulkanInstance)), functionName);
            }, &interfaceDevice->renderInterface->instance);

            std::unordered_map<VkDescriptorType, uint32_t> typeCounts;
            typeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 1;
            
            vulkanContext = std::make_unique<VulkanContext>();
            vulkanContext->device = interfaceDevice->vk;
            vulkanContext->renderPass = VulkanGraphicsPipeline::createRenderPass(interfaceDevice, &interfaceSwapChain->pickedSurfaceFormat.format, 1, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT);

            ImGui_ImplVulkan_InitInfo initInfo = {};
            initInfo.Instance = interfaceDevice->renderInterface->instance;
            initInfo.PhysicalDevice = interfaceDevice->physicalDevice;
            initInfo.Device = vulkanContext->device;
            initInfo.QueueFamily = interfaceSwapChain->commandQueue->familyIndex;
            initInfo.Queue = interfaceSwapChain->commandQueue->queue->vk;
            initInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
            initInfo.MinImageCount = 2;
            initInfo.ImageCount = 2;
            initInfo.CheckVkResultFn = &checkVulkanResult;
            initInfo.PipelineInfoMain.RenderPass = vulkanContext->renderPass;

            ImGui_ImplVulkan_Init(&initInfo);
            break;
        }
        default:
            assert(false && "Unknown Graphics API.");
            break;
    }

    SDL_AddEventWatch(sdl_event_filter, nullptr);

    ImGui::SetCurrentContext(prev_ctx);
}

static void rt64_init_hook(plume::RenderInterface* _interface, plume::RenderDevice* device) { }

static void rt64_draw_hook(plume::RenderCommandList* command_list, plume::RenderFramebuffer* swap_chain_framebuffer) {
    if (!b_is_open) return;
    if (dino_imgui_ctx == nullptr) return;
    
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
            default:
                assert(false && "Unknown Graphics API.");
                break;
        }
    }

    ImGui::SetCurrentContext(prev_ctx);
}

static void rt64_deinit_hook() {
    if (dino_imgui_ctx == nullptr) return;

    const std::lock_guard<std::mutex> frame_lock(frame_mutex);
    assert(!b_in_ui_frame);
    b_is_open = false;

    ImGuiContext *prev_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(dino_imgui_ctx);

    switch (get_graphics_api()) {
        case RT64::UserConfiguration::GraphicsAPI::D3D12:
#ifdef _WIN32
            ImGui_ImplDX12_Shutdown();
            descriptorSet.reset(nullptr);
#else
            assert(false && "Unsupported Graphics API.");
            return;
#endif
            break;
        case RT64::UserConfiguration::GraphicsAPI::Vulkan:
            ImGui_ImplVulkan_Shutdown();
            vulkanContext.reset(nullptr);
            break;
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
