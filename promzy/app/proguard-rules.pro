# kotlinx.serialization
-keepattributes *Annotation*, InnerClasses
-dontnote kotlinx.serialization.**
-keepclassmembers class kotlinx.serialization.json.** { *** Companion; }
-keepclasseswithmembers class kotlinx.serialization.json.** { kotlinx.serialization.KSerializer serializer(...); }
-keep,includedescriptorclasses class com.superessence.promzy.**$$serializer { *; }
-keepclassmembers class com.superessence.promzy.** { *** Companion; }
-keepclasseswithmembers class com.superessence.promzy.** { kotlinx.serialization.KSerializer serializer(...); }
-keep @kotlinx.serialization.Serializable class com.superessence.promzy.** { *; }
