# Whisk3D en Android

Backend GLES1 + SDL2, empaquetado como APK con Gradle.

## Requisitos

- **Android NDK** (define `ANDROID_NDK` o dejalo en `/opt/android-ndk`).
- **JDK 17** + el `gradlew` que ya viene en `platform/android/`. Gradle necesita `JAVA_HOME` apuntando al JDK
  (si no, falla con *"JAVA_HOME is not set"*), por ejemplo:
  `export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64`

## Compilar (lo más fácil)

Desde la raíz del repo:

```bash
platform/android/build_android.sh
```

El script:
1. Copia `res/` y `platform/android/templates/` a los assets del APK.
2. Compila las librerías nativas con `ndk-build` para las ABIs (por defecto `arm64`, `amd64`, `arm32`).
3. Empaqueta el APK con `./gradlew assembleDebug` (firmado con el keystore de debug).

La primera vez que se abre la app, los proyectos de ejemplo se copian a su carpeta privada
`files/Templates` y aparecen como acceso **Templates** en el explorador de archivos. Se pueden
abrir desde ahí y guardar una copia en Descargas con **Save As**.
Al elegir **New Project** en la pantalla de bienvenida, se puede crear un proyecto vacío o
seleccionar Animation usage, Control usage, Platformer game o Audio usage. Después se elige una
carpeta compartida de destino; Whisk3D crea una subcarpeta nueva con una copia editable de la
plantilla y abre ese proyecto.

Para una sola ABI: `platform/android/build_android.sh arm64`.

## A mano (solo el APK, si ya están las `.so`)

```bash
cd platform/android && ./gradlew assembleDebug
```

## Salida

El APK queda en `platform/android/app/distribution/android/app/outputs/apk/debug/whisk3d-*-Android.apk`.
