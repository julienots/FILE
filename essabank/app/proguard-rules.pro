# kotlinx.serialization (modèle du moteur ESSABANK)
-keepattributes *Annotation*, InnerClasses
-dontnote kotlinx.serialization.**
-keepclassmembers class kotlinx.serialization.json.** { *** Companion; }
-keepclasseswithmembers class kotlinx.serialization.json.** { kotlinx.serialization.KSerializer serializer(...); }
-keep,includedescriptorclasses class com.superessence.essabank.**$$serializer { *; }
-keepclassmembers class com.superessence.essabank.** { *** Companion; }
-keepclasseswithmembers class com.superessence.essabank.** { kotlinx.serialization.KSerializer serializer(...); }
-keep @kotlinx.serialization.Serializable class com.superessence.essabank.** { *; }
