package com.superessence.promzy

import com.superessence.promzy.data.Deal

val SampleRss = """<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:media="http://search.yahoo.com/mrss/" xmlns:pepper="https://www.pepper.com/rss" xmlns:dc="http://purl.org/dc/elements/1.1/">
<channel><title>Dealabs - Hot</title><link>https://www.dealabs.com</link>
<item>
  <title><![CDATA[Console Sony PlayStation 5 Slim Digital à 349,99€ (au lieu de 449,99€)]]></title>
  <link>https://www.dealabs.com/bons-plans/ps5-slim-123456</link>
  <guid isPermaLink="false">deal-123456</guid>
  <pubDate>Sun, 22 Nov 2026 20:15:00 +0100</pubDate>
  <category><![CDATA[Consoles]]></category>
  <category>Jeux vidéo</category>
  <description><![CDATA[<img src="https://static-pepper.dealabs.com/threads/raw/abc/123456_1/re/300x300/qt/60/123456_1.jpg" alt=""/><strong>Prix : 349,99€</strong><br/>Super prix pour la PS5 &amp; ses 1 To. 1 254°]]></description>
  <media:content medium="image" url="https://static-pepper.dealabs.com/threads/raw/abc/123456_1/re/1024x1024/123456_1.jpg"/>
  <pepper:merchant name="Amazon" price="349.99"/>
  <dc:creator>chasseur</dc:creator>
</item>
<item>
  <title>Jeu Epic Games offert</title>
  <link>https://www.dealabs.com/gratuit/epic-777</link>
  <pubDate>Sun, 22 Nov 2026 19:00:00 +0100</pubDate>
  <description>Récupérez le jeu gratuitement</description>
  <pepper:merchant name="Epic Games" price="0"/>
</item>
<item><title>Sans lien</title></item>
</channel></rss>"""

private val now = System.currentTimeMillis()
val SampleDeals = listOf(
    Deal("1", "Console Sony PlayStation 5 Slim Digital Edition — 1 To", "https://x/1", null, "349,99€", "449,99€", "Amazon", now - 12 * 60_000, "Consoles", temperature = 1254),
    Deal("2", "Apple AirPods Pro 2 (USB-C) avec boîtier MagSafe", "https://x/2", null, "189€", "279€", "Fnac", now - 40 * 60_000, "Audio", temperature = 812),
    Deal("3", "Aspirateur robot Roborock Q7 Max+ avec station d'auto-vidage", "https://x/3", null, "259,99€", null, "Cdiscount", now - 2 * 3600_000, "Maison", temperature = 431),
    Deal("4", "Jeu PC Hogwarts Legacy offert sur l'Epic Games Store", "https://x/4", null, "Gratuit", null, "Epic Games", now - 3 * 3600_000, "Jeux vidéo", temperature = 377),
    Deal("5", "TV OLED LG 55\" OLED55C4 4K 120 Hz", "https://x/5", null, "899€", "1 290€", "Boulanger", now - 5 * 3600_000, "TV", temperature = 268),
    Deal("6", "Baskets Nike Air Max 90 (du 38 au 46)", "https://x/6", null, "79,99€", "149,99€", "Nike", now - 6 * 3600_000, "Mode", temperature = 145),
    Deal("7", "Lot de 100 capsules café compatibles Nespresso", "https://x/7", null, "14,90€", null, "Carrefour", now - 8 * 3600_000, "Courses", temperature = 62),
)
