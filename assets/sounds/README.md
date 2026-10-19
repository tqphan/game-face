# Sounds

Ten sounds from Kenney's [UI Audio](https://kenney.nl/assets/ui-audio) pack
(CC0, see `License.txt`), played when a binding's start or stop fires
(Settings → Sounds).

They were converted from the pack's `Audio/*.ogg` (`kenney_ui-audio.zip`,
SHA-256 `946fc23a63d535d693eb31b2eabb80c8c28d6351e2186b344ceb71b2cb1d5eb6`) to
16-bit 44.1 kHz stereo WAV, because `QSoundEffect` plays only WAV. The names
are the pack's. To add one, convert it the same way, add it here and to
`kSounds` in `app/ui/sound_player.cpp`, and list it in
`app/ui/CMakeLists.txt`.
