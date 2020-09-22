# Rum and Raisin Doom

Rum and Raisin Doom is a fork of Chocolate Doom focusing solely on modernising the renderer.
It aims to be exactly as Vanilla compatible as Chocolate Doom, but with optimisations better
suited to modern hardware than the standard renderers found in source ports.

The joke in the name explains the ethos - we're chocolate, but with additions you may or may
not like.

The ultimate goal is preservation of the software rendering style at high resolutions. This
includes optimising the original renderer for modern architectures; and will also include
GPU implementations that preserve the aesthetic and bugs of the software renderer.

The perservation effort will also assume that resolution-specific implementations are stylistic
choices. Thus, certain effects will scale. The screen wipe is the most immediately obvious,
retaining the original 160x200 scrolling behaviour.

Just as Carmack wrote the original renderer to the metal to exploit every hardware advantage,
this port will also do the same. Occasionally, this results in minor differences to the original
renderer. Partial invisibility is the first such casualty, as now the effect does not happen on
the first and last columns instead of the first and last rows.
