#!/bin/sh
# Assumes a normal cmake build was already done

# Make AppDir directories
mkdir -p AppDir/usr/bin/
mkdir -p AppDir/usr/share/metainfo/

# AppStream Metainfo
cp ../io.github.dinosaurplanetrecomp.dino_recomp.metainfo.xml AppDir/usr/share/metainfo/

# Assets
cp -R ../../assets/ AppDir/usr/bin/
rm -rf AppDir/usr/bin/assets/scss/

# gamecontrollerdb
cp ../../recompcontrollerdb.txt AppDir/usr/bin/

NO_STRIP=true linuxdeploy.AppImage --appdir AppDir \
    --executable ../../build/DinosaurPlanetRecompiled \
    --desktop-file ../io.github.dinosaurplanetrecomp.dino_recomp.desktop \
    --icon-file ../../icons/64.png \
    --icon-filename io.github.dinosaurplanetrecomp.dino_recomp \
    --output appimage
mv Dinosaur_Planet:_Recompiled-x86_64.AppImage DinosaurPlanetRecompiled-x86_64.AppImage
