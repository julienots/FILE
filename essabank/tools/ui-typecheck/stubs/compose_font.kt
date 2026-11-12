@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.compose.ui.text.font
private val names = mapOf(1 to "manrope_500", 2 to "manrope_600", 3 to "manrope_700", 4 to "manrope_800", 5 to "inter_400", 6 to "inter_500", 7 to "inter_600", 8 to "inter_700")
fun Font(resId: Int, weight: FontWeight = FontWeight.Normal, style: FontStyle = FontStyle.Normal): Font =
    androidx.compose.ui.text.platform.Font(java.io.File("../../app/src/main/res/font/" + names[resId] + ".ttf"), weight, style)
