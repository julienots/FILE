# kotlinx.serialization
-keepattributes *Annotation*, InnerClasses
-dontnote kotlinx.serialization.**
-keepclassmembers class kotlinx.serialization.json.** { *** Companion; }
-keepclasseswithmembers class kotlinx.serialization.json.** { kotlinx.serialization.KSerializer serializer(...); }
-keep,includedescriptorclasses class com.superessence.noctea.**$$serializer { *; }
-keepclassmembers class com.superessence.noctea.** { *** Companion; }
-keepclasseswithmembers class com.superessence.noctea.** { kotlinx.serialization.KSerializer serializer(...); }
-keep @kotlinx.serialization.Serializable class com.superessence.noctea.** { *; }
