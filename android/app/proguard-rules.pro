# ProGuard rules for Lux Engine
# Keep JNI native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep the engine package
-keep class com.lux.engine.** { *; }

# Keep Kotlin companion objects with JNI
-keepclasseswithmembers class * {
    companion val *;
}
