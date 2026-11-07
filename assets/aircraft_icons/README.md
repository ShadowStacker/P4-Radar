# Aircraft icons

Icons by ADS-B Radar for macOS - https://adsb-radar.com - https://apps.apple.com/app/id1538149835

These SVG files come from the free icon set at https://adsb-radar.com/help/icons.html,
which may be used in personal and commercial projects with a backlink to ADS-B Radar.

`scripts/gen_aircraft_icons.js` renders them into `main/data/aircraft_icons_data.cpp`
(compressed 128 px alpha masks); the radar rotates and colors them at run time.
Which aircraft types get which icon is in `main/core/aircraft_types.cpp`.
