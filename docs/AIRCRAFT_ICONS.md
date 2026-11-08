# Aircraft icons

The radar draws each aircraft with an icon for its type: **388 icons** for **713 ICAO type codes**. This page lists them all, generated from the firmware's own table (`scripts/gen_icon_docs.py`).

**How an icon is picked.** The feed gives most aircraft an ICAO type designator (`B738`, `C172`, `H60`). If the code is in the table, that icon is used; close variants share one (737-800, MAX 8 and P-8 are one icon). If the feed has no type, or one not in the table, the aircraft gets a [size class](#size-classes-when-the-type-is-unknown) icon from its ADS-B emitter category (light, large, heavy, rotorcraft, glider...).

**Size.** Icons are drawn at the sizes below (pixels on the 800 px screen at 100 % aircraft size), from the aircraft's real size: a Robinson R22 is small, an A380 big. *Settings > Display > Aircraft size* scales them all, and zoomed out with many aircraft a smaller set is used.

**Credits.** Icons by ADS-B Radar for macOS - https://adsb-radar.com - https://apps.apple.com/app/id1538149835 (the size classes and a few others below). The rest are drawn in the same style from each aircraft's real dimensions by `scripts/aircraft_catalog.py` and `scripts/aircraft_families.py`.

Missing a type, or a type with the wrong icon? The table is `scripts/gen_aircraft_types.py` and `scripts/aircraft_catalog.py`; `scripts/check_icon_coverage.py` lists live types without one.

## Contents

- [Airliners and regional jets](#airliners-and-regional-jets) (72)
- [Regional turboprops](#regional-turboprops) (17)
- [Business jets](#business-jets) (44)
- [Light aircraft: singles](#light-aircraft-singles) (43)
- [Light aircraft: twins and utility](#light-aircraft-twins-and-utility) (30)
- [Classic airliners and transports](#classic-airliners-and-transports) (15)
- [Warbirds and classic fighters](#warbirds-and-classic-fighters) (23)
- [Experimental and aerobatic](#experimental-and-aerobatic) (8)
- [Military jets](#military-jets) (36)
- [Military transports, bombers, tankers and patrol](#military-transports-bombers-tankers-and-patrol) (14)
- [Helicopters](#helicopters) (53)
- [Families (shared shapes)](#families-shared-shapes) (17)
- [Other ADS-B Radar icons](#other-ads-b-radar-icons) (3)
- [Size classes (when the type is unknown)](#size-classes-when-the-type-is-unknown) (13)

## Airliners and regional jets

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_a318.svg" width="48" height="48" alt="x_a318"> | Airbus A318 | `A318` | 42 px |
| <img src="icons/x_a319.svg" width="48" height="48" alt="x_a319"> | Airbus A319 | `A19N` `A319` | 43 px |
| <img src="icons/x_a321.svg" width="48" height="48" alt="x_a321"> | Airbus A321 | `A21N` `A321` | 46 px |
| <img src="icons/x_a220_100.svg" width="48" height="48" alt="x_a220_100"> | Airbus A220-100 | `BCS1` | 42 px |
| <img src="icons/x_a220_300.svg" width="48" height="48" alt="x_a220_300"> | Airbus A220-300 | `BCS3` | 44 px |
| <img src="icons/x_c919.svg" width="48" height="48" alt="x_c919"> | COMAC C919 | `C919` | 44 px |
| <img src="icons/x_b732.svg" width="48" height="48" alt="x_b732"> | Boeing 737-200 | `B731` `B732` | 41 px |
| <img src="icons/x_b733.svg" width="48" height="48" alt="x_b733"> | Boeing 737-300 | `B733` | 42 px |
| <img src="icons/x_b734.svg" width="48" height="48" alt="x_b734"> | Boeing 737-400 | `B734` | 43 px |
| <img src="icons/x_b735.svg" width="48" height="48" alt="x_b735"> | Boeing 737-500 | `B735` | 41 px |
| <img src="icons/x_b736.svg" width="48" height="48" alt="x_b736"> | Boeing 737-600 | `B736` | 42 px |
| <img src="icons/x_b737.svg" width="48" height="48" alt="x_b737"> | Boeing 737-700 / MAX 7 | `B37M` `B737` `E737` | 42 px |
| <img src="icons/x_b739.svg" width="48" height="48" alt="x_b739"> | Boeing 737-900 / MAX 9 / MAX 10 | `B39M` `B3XM` `B739` | 45 px |
| <img src="icons/x_b752.svg" width="48" height="48" alt="x_b752"> | Boeing 757-200 | `B752` | 46 px |
| <img src="icons/x_b753.svg" width="48" height="48" alt="x_b753"> | Boeing 757-300 | `B753` | 48 px |
| <img src="icons/x_b762.svg" width="48" height="48" alt="x_b762"> | Boeing 767-200 | `B762` | 47 px |
| <img src="icons/x_b764.svg" width="48" height="48" alt="x_b764"> | Boeing 767-400 | `B764` | 50 px |
| <img src="icons/x_b772.svg" width="48" height="48" alt="x_b772"> | Boeing 777-200 | `B772` `B77L` | 51 px |
| <img src="icons/x_b773.svg" width="48" height="48" alt="x_b773"> | Boeing 777-300 | `B773` | 53 px |
| <img src="icons/x_b77w.svg" width="48" height="48" alt="x_b77w"> | Boeing 777-300ER | `B77W` | 53 px |
| <img src="icons/x_b779.svg" width="48" height="48" alt="x_b779"> | Boeing 777X | `B778` `B779` | 54 px |
| <img src="icons/x_b788.svg" width="48" height="48" alt="x_b788"> | Boeing 787-8 | `B788` | 50 px |
| <img src="icons/x_b78x.svg" width="48" height="48" alt="x_b78x"> | Boeing 787-10 | `B78X` | 52 px |
| <img src="icons/x_a300.svg" width="48" height="48" alt="x_a300"> | Airbus A300 | `A306` `A30B` | 48 px |
| <img src="icons/x_a310.svg" width="48" height="48" alt="x_a310"> | Airbus A310 | `A310` | 46 px |
| <img src="icons/x_a332.svg" width="48" height="48" alt="x_a332"> | Airbus A330-200 | `A332` | 50 px |
| <img src="icons/x_a339.svg" width="48" height="48" alt="x_a339"> | Airbus A330neo | `A338` `A339` | 51 px |
| <img src="icons/x_a359.svg" width="48" height="48" alt="x_a359"> | Airbus A350-900 | `A359` | 52 px |
| <img src="icons/x_a35k.svg" width="48" height="48" alt="x_a35k"> | Airbus A350-1000 | `A35K` | 53 px |
| <img src="icons/x_a346.svg" width="48" height="48" alt="x_a346"> | Airbus A340-500/600 | `A345` `A346` | 53 px |
| <img src="icons/x_b748.svg" width="48" height="48" alt="x_b748"> | Boeing 747-8 | `B748` | 54 px |
| <img src="icons/x_b74s.svg" width="48" height="48" alt="x_b74s"> | Boeing 747SP | `B74S` | 50 px |
| <img src="icons/x_beluga.svg" width="48" height="48" alt="x_beluga"> | Airbus Beluga | `A3ST` | 49 px |
| <img src="icons/x_belugaxl.svg" width="48" height="48" alt="x_belugaxl"> | Airbus BelugaXL | `A337` | 51 px |
| <img src="icons/x_blcf.svg" width="48" height="48" alt="x_blcf"> | Boeing Dreamlifter | `BLCF` | 53 px |
| <img src="icons/x_dc10.svg" width="48" height="48" alt="x_dc10"> | McDonnell Douglas DC-10 / KC-10 | `DC10` | 49 px |
| <img src="icons/x_l1011.svg" width="48" height="48" alt="x_l1011"> | Lockheed L-1011 TriStar | `L101` | 48 px |
| <img src="icons/x_b712.svg" width="48" height="48" alt="x_b712"> | Boeing 717 | `B712` | 43 px |
| <img src="icons/x_md80.svg" width="48" height="48" alt="x_md80"> | McDonnell Douglas MD-80 | `MD81` `MD82` `MD83` `MD87` `MD88` | 46 px |
| <img src="icons/x_md90.svg" width="48" height="48" alt="x_md90"> | McDonnell Douglas MD-90 | `MD90` | 46 px |
| <img src="icons/x_dc9.svg" width="48" height="48" alt="x_dc9"> | Douglas DC-9 | `DC91` `DC92` `DC93` `DC94` `DC95` | 43 px |
| <img src="icons/x_f100.svg" width="48" height="48" alt="x_f100"> | Fokker 100 | `F100` | 43 px |
| <img src="icons/x_f70.svg" width="48" height="48" alt="x_f70"> | Fokker 70 | `F70` | 41 px |
| <img src="icons/x_b722.svg" width="48" height="48" alt="x_b722"> | Boeing 727-200 | `B722` | 46 px |
| <img src="icons/x_b721.svg" width="48" height="48" alt="x_b721"> | Boeing 727-100 | `B721` | 44 px |
| <img src="icons/x_bae146.svg" width="48" height="48" alt="x_bae146"> | BAe 146 / Avro RJ85 | `B461` `B462` `RJ70` `RJ85` | 40 px |
| <img src="icons/x_bae146_300.svg" width="48" height="48" alt="x_bae146_300"> | BAe 146-300 / Avro RJ100 | `B463` `RJ1H` | 41 px |
| <img src="icons/x_t154.svg" width="48" height="48" alt="x_t154"> | Tupolev Tu-154 | `T154` | 47 px |
| <img src="icons/x_il62.svg" width="48" height="48" alt="x_il62"> | Ilyushin Il-62 | `IL62` | 48 px |
| <img src="icons/x_yk42.svg" width="48" height="48" alt="x_yk42"> | Yakovlev Yak-42 | `YK42` | 43 px |
| <img src="icons/x_crj2.svg" width="48" height="48" alt="x_crj2"> | Bombardier CRJ100/200 | `CRJ1` `CRJ2` | 39 px |
| <img src="icons/x_crj7.svg" width="48" height="48" alt="x_crj7"> | Bombardier CRJ700 | `CRJ7` | 41 px |
| <img src="icons/x_e170.svg" width="48" height="48" alt="x_e170"> | Embraer 170 | `E170` | 40 px |
| <img src="icons/x_e175.svg" width="48" height="48" alt="x_e175"> | Embraer 175 | `E175` `E75L` `E75S` | 41 px |
| <img src="icons/x_e190.svg" width="48" height="48" alt="x_e190"> | Embraer 190 | `E190` | 43 px |
| <img src="icons/x_e290.svg" width="48" height="48" alt="x_e290"> | Embraer E190-E2 | `E290` | 43 px |
| <img src="icons/x_e135.svg" width="48" height="48" alt="x_e135"> | Embraer ERJ 135 / Legacy | `E135` `E35L` | 39 px |
| <img src="icons/x_su95.svg" width="48" height="48" alt="x_su95"> | Sukhoi Superjet 100 | `SU95` | 40 px |
| <img src="icons/x_arj21.svg" width="48" height="48" alt="x_arj21"> | COMAC ARJ21 | `AJ27` | 42 px |
| <img src="icons/x_a320.svg" width="48" height="48" alt="x_a320"> | Airbus A320 / A320neo | `A20N` `A320` | 43 px |
| <img src="icons/x_b738.svg" width="48" height="48" alt="x_b738"> | Boeing 737-800 / MAX 8 / P-8 | `B38M` `B738` `P8` | 44 px |
| <img src="icons/x_b789.svg" width="48" height="48" alt="x_b789"> | Boeing 787-9 | `B789` | 51 px |
| <img src="icons/x_crj9.svg" width="48" height="48" alt="x_crj9"> | Bombardier CRJ900/1000 | `CRJ9` `CRJX` | 43 px |
| <img src="icons/x_b763.svg" width="48" height="48" alt="x_b763"> | Boeing 767-300 | `B763` | 49 px |
| <img src="icons/x_a333.svg" width="48" height="48" alt="x_a333"> | Airbus A330-300 | `A333` | 51 px |
| <img src="icons/x_e195.svg" width="48" height="48" alt="x_e195"> | Embraer 195 | `E195` | 44 px |
| <img src="icons/x_e295.svg" width="48" height="48" alt="x_e295"> | Embraer E195-E2 | `E295` | 45 px |
| <img src="icons/x_e145.svg" width="48" height="48" alt="x_e145"> | Embraer ERJ 145 | `E145` `E45X` | 40 px |
| <img src="icons/x_b744.svg" width="48" height="48" alt="x_b744"> | Boeing 747-400 / classic | `B741` `B742` `B743` `B744` `B74D` `B74R` | 52 px |
| <img src="icons/x_a388.svg" width="48" height="48" alt="x_a388"> | Airbus A380 | `A388` | 54 px |
| <img src="icons/x_a343.svg" width="48" height="48" alt="x_a343"> | Airbus A340-200/300 | `A342` `A343` | 51 px |
| <img src="icons/x_md11.svg" width="48" height="48" alt="x_md11"> | McDonnell Douglas MD-11 | `MD11` | 50 px |

## Regional turboprops

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_dh8c.svg" width="48" height="48" alt="x_dh8c"> | Dash 8-300 | `DH8C` | 39 px |
| <img src="icons/x_dh8d.svg" width="48" height="48" alt="x_dh8d"> | Dash 8 Q400 | `DH8D` | 42 px |
| <img src="icons/x_atr42.svg" width="48" height="48" alt="x_atr42"> | ATR 42 | `AT43` `AT44` `AT45` `AT46` | 38 px |
| <img src="icons/x_atr72.svg" width="48" height="48" alt="x_atr72"> | ATR 72 | `AT72` `AT73` `AT75` `AT76` | 39 px |
| <img src="icons/x_sf34.svg" width="48" height="48" alt="x_sf34"> | Saab 340 | `SF34` | 37 px |
| <img src="icons/x_sb20.svg" width="48" height="48" alt="x_sb20"> | Saab 2000 | `SB20` | 39 px |
| <img src="icons/x_d328.svg" width="48" height="48" alt="x_d328"> | Dornier 328 | `D328` | 37 px |
| <img src="icons/x_j328.svg" width="48" height="48" alt="x_j328"> | Dornier 328JET | `J328` | 37 px |
| <img src="icons/x_e120.svg" width="48" height="48" alt="x_e120"> | Embraer Brasilia | `E120` | 36 px |
| <img src="icons/x_js41.svg" width="48" height="48" alt="x_js41"> | Jetstream 41 | `JS41` | 36 px |
| <img src="icons/x_f50.svg" width="48" height="48" alt="x_f50"> | Fokker 50 / F27 | `F27` `F50` | 40 px |
| <img src="icons/x_dhc7.svg" width="48" height="48" alt="x_dhc7"> | Dash 7 | `DHC7` | 40 px |
| <img src="icons/x_an26.svg" width="48" height="48" alt="x_an26"> | Antonov An-24/26/32, MA60 | `AN24` `AN26` `AN32` `MA60` | 40 px |
| <img src="icons/x_c295.svg" width="48" height="48" alt="x_c295"> | CASA C-295 | `C295` | 39 px |
| <img src="icons/x_cn235.svg" width="48" height="48" alt="x_cn235"> | CASA CN-235 | `CN35` | 39 px |
| <img src="icons/x_b190.svg" width="48" height="48" alt="x_b190"> | Beech 1900 | `B190` | 35 px |
| <img src="icons/x_dh8a.svg" width="48" height="48" alt="x_dh8a"> | Dash 8-100/200 | `DH8A` `DH8B` | 39 px |

## Business jets

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_c510.svg" width="48" height="48" alt="x_c510"> | Citation Mustang | `C510` | 32 px |
| <img src="icons/x_c525.svg" width="48" height="48" alt="x_c525"> | CitationJet CJ1/M2 | `C25M` `C525` `C526` | 32 px |
| <img src="icons/x_c25a.svg" width="48" height="48" alt="x_c25a"> | Citation CJ2 | `C25A` | 33 px |
| <img src="icons/x_c25b.svg" width="48" height="48" alt="x_c25b"> | Citation CJ3 | `C25B` | 34 px |
| <img src="icons/x_c25c.svg" width="48" height="48" alt="x_c25c"> | Citation CJ4 | `C25C` | 34 px |
| <img src="icons/x_c500.svg" width="48" height="48" alt="x_c500"> | Citation I/II/Bravo | `C500` `C501` `C550` `C551` `C55B` | 34 px |
| <img src="icons/x_c560.svg" width="48" height="48" alt="x_c560"> | Citation V/Ultra/Encore | `C560` | 34 px |
| <img src="icons/x_c56x.svg" width="48" height="48" alt="x_c56x"> | Citation Excel/XLS | `C56X` | 34 px |
| <img src="icons/x_c680.svg" width="48" height="48" alt="x_c680"> | Citation Sovereign | `C680` | 36 px |
| <img src="icons/x_c68a.svg" width="48" height="48" alt="x_c68a"> | Citation Latitude | `C68A` | 37 px |
| <img src="icons/x_c700.svg" width="48" height="48" alt="x_c700"> | Citation Longitude | `C700` | 37 px |
| <img src="icons/x_c750.svg" width="48" height="48" alt="x_c750"> | Citation X | `C750` | 37 px |
| <img src="icons/x_lj35.svg" width="48" height="48" alt="x_lj35"> | Learjet 31/35/36 | `LJ31` `LJ35` `LJ36` | 33 px |
| <img src="icons/x_lj45.svg" width="48" height="48" alt="x_lj45"> | Learjet 40/45/70/75 | `LJ40` `LJ45` `LJ70` `LJ75` | 35 px |
| <img src="icons/x_lj60.svg" width="48" height="48" alt="x_lj60"> | Learjet 55/60 | `LJ55` `LJ60` | 35 px |
| <img src="icons/x_e50p.svg" width="48" height="48" alt="x_e50p"> | Phenom 100 | `E50P` | 31 px |
| <img src="icons/x_e55p.svg" width="48" height="48" alt="x_e55p"> | Phenom 300 | `E55P` | 34 px |
| <img src="icons/x_e545.svg" width="48" height="48" alt="x_e545"> | Praetor 500 / Legacy 450 | `E545` | 36 px |
| <img src="icons/x_e550.svg" width="48" height="48" alt="x_e550"> | Praetor 600 / Legacy 500 | `E550` | 37 px |
| <img src="icons/x_hdjt.svg" width="48" height="48" alt="x_hdjt"> | HondaJet | `HDJT` | 32 px |
| <img src="icons/x_sf50.svg" width="48" height="48" alt="x_sf50"> | Cirrus Vision Jet | `SF50` | 31 px |
| <img src="icons/x_pc24.svg" width="48" height="48" alt="x_pc24"> | Pilatus PC-24 | `PC24` | 34 px |
| <img src="icons/x_be40.svg" width="48" height="48" alt="x_be40"> | Beechjet 400 / Hawker 400 | `BE40` | 33 px |
| <img src="icons/x_prm1.svg" width="48" height="48" alt="x_prm1"> | Beech Premier | `PRM1` | 32 px |
| <img src="icons/x_h25b.svg" width="48" height="48" alt="x_h25b"> | Hawker 800/900 | `H25A` `H25B` `H25C` | 33 px |
| <img src="icons/x_g280.svg" width="48" height="48" alt="x_g280"> | Gulfstream G280 | `G280` | 36 px |
| <img src="icons/x_glf4.svg" width="48" height="48" alt="x_glf4"> | Gulfstream IV/G450 | `GLF4` | 39 px |
| <img src="icons/x_glf6.svg" width="48" height="48" alt="x_glf6"> | Gulfstream G650 | `GLF6` | 41 px |
| <img src="icons/x_ga7c.svg" width="48" height="48" alt="x_ga7c"> | Gulfstream G700/G800/G500/G600 | `GA5C` `GA6C` `GA7C` `GA8C` | 42 px |
| <img src="icons/x_glex.svg" width="48" height="48" alt="x_glex"> | Bombardier Global Express/6000 | `GLEX` | 41 px |
| <img src="icons/x_gl5t.svg" width="48" height="48" alt="x_gl5t"> | Bombardier Global 5000/5500 | `GL5T` | 40 px |
| <img src="icons/x_gl7t.svg" width="48" height="48" alt="x_gl7t"> | Bombardier Global 7500/8000 | `GL7T` `GL8T` | 42 px |
| <img src="icons/x_cl30.svg" width="48" height="48" alt="x_cl30"> | Challenger 300/350 | `CL30` `CL35` | 36 px |
| <img src="icons/x_cl60.svg" width="48" height="48" alt="x_cl60"> | Challenger 600/650 | `CL60` | 36 px |
| <img src="icons/x_fa8x.svg" width="48" height="48" alt="x_fa8x"> | Falcon 8X | `FA8X` | 39 px |
| <img src="icons/x_f2th.svg" width="48" height="48" alt="x_f2th"> | Falcon 2000 | `F2TH` | 37 px |
| <img src="icons/x_fa50.svg" width="48" height="48" alt="x_fa50"> | Falcon 50/900 | `F900` `FA50` | 36 px |
| <img src="icons/x_fa6x.svg" width="48" height="48" alt="x_fa6x"> | Falcon 6X | `FA6X` | 39 px |
| <img src="icons/x_fa20.svg" width="48" height="48" alt="x_fa20"> | Falcon 10/20 | `FA10` `FA20` | 34 px |
| <img src="icons/x_fa7x.svg" width="48" height="48" alt="x_fa7x"> | Falcon 7X | `FA7X` | 39 px |
| <img src="icons/x_glf5.svg" width="48" height="48" alt="x_glf5"> | Gulfstream II/III/V/G550 | `GLF2` `GLF3` `GLF5` | 40 px |
| <img src="icons/x_g150.svg" width="48" height="48" alt="x_g150"> | Gulfstream G100/G150/G200 | `ASTR` `G150` `GALX` | 34 px |
| <img src="icons/x_c650.svg" width="48" height="48" alt="x_c650"> | Citation III/VI/VII | `C650` | 34 px |
| <img src="icons/x_ea50.svg" width="48" height="48" alt="x_ea50"> | Eclipse 500/550 | `EA50` | 30 px |

## Light aircraft: singles

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_c152.svg" width="48" height="48" alt="x_c152"> | Cessna 150/152 | `C150` `C152` | 29 px |
| <img src="icons/x_c172.svg" width="48" height="48" alt="x_c172"> | Cessna 172 | `C172` `C72R` | 30 px |
| <img src="icons/x_c182.svg" width="48" height="48" alt="x_c182"> | Cessna 182/206 | `C182` `C205` `C206` `C207` `C82R` `C82S` `C82T` `T206` | 30 px |
| <img src="icons/x_c210.svg" width="48" height="48" alt="x_c210"> | Cessna 210 | `C210` `P210` `T210` | 30 px |
| <img src="icons/x_c177.svg" width="48" height="48" alt="x_c177"> | Cessna Cardinal | `C177` `C77R` | 30 px |
| <img src="icons/x_c180.svg" width="48" height="48" alt="x_c180"> | Cessna 180/185/170 (taildragger) | `C170` `C180` `C185` `C195` | 30 px |
| <img src="icons/x_pa18.svg" width="48" height="48" alt="x_pa18"> | Piper Super Cub | `PA12` `PA14` `PA18` `PA20` `PA22` | 30 px |
| <img src="icons/x_j3.svg" width="48" height="48" alt="x_j3"> | Piper J-3 Cub | `J3` | 30 px |
| <img src="icons/x_bl8.svg" width="48" height="48" alt="x_bl8"> | Citabria / Decathlon | `BL8` `CH7A` `CH7B` | 30 px |
| <img src="icons/x_husk.svg" width="48" height="48" alt="x_husk"> | Aviat Husky | `HUSK` | 30 px |
| <img src="icons/x_m7.svg" width="48" height="48" alt="x_m7"> | Maule | `M5` `M6` `M7` `MT7` | 29 px |
| <img src="icons/x_ga8.svg" width="48" height="48" alt="x_ga8"> | GippsAero Airvan | `GA8` | 31 px |
| <img src="icons/x_pa32.svg" width="48" height="48" alt="x_pa32"> | Piper Saratoga/Cherokee Six | `P32R` `P32T` `PA32` | 30 px |
| <img src="icons/x_pa38.svg" width="48" height="48" alt="x_pa38"> | Piper Tomahawk | `PA38` | 29 px |
| <img src="icons/x_pa24.svg" width="48" height="48" alt="x_pa24"> | Piper Comanche | `PA24` | 30 px |
| <img src="icons/x_pa46.svg" width="48" height="48" alt="x_pa46"> | Piper Malibu/Meridian/M500-M700 | `M700` `P46T` `PA46` | 32 px |
| <img src="icons/x_sr22.svg" width="48" height="48" alt="x_sr22"> | Cirrus SR20/SR22 | `S22T` `SR20` `SR22` | 31 px |
| <img src="icons/x_da40.svg" width="48" height="48" alt="x_da40"> | Diamond DA40 | `DA40` | 31 px |
| <img src="icons/x_da20.svg" width="48" height="48" alt="x_da20"> | Diamond DA20 | `DA20` `DV20` | 30 px |
| <img src="icons/x_be35.svg" width="48" height="48" alt="x_be35"> | Beech Bonanza V-tail | `BE35` | 29 px |
| <img src="icons/x_be36.svg" width="48" height="48" alt="x_be36"> | Beech Bonanza A36 | `BE33` `BE36` `BT36` | 29 px |
| <img src="icons/x_m20.svg" width="48" height="48" alt="x_m20"> | Mooney M20 | `M20J` `M20K` `M20P` `M20R` `M20T` | 30 px |
| <img src="icons/x_rv7.svg" width="48" height="48" alt="x_rv7"> | Van's RV-6/7/8/9/14 | `RV14` `RV4` `RV6` `RV7` `RV8` `RV9` | 27 px |
| <img src="icons/x_rv10.svg" width="48" height="48" alt="x_rv10"> | Van's RV-10 | `RV10` | 29 px |
| <img src="icons/x_rv12.svg" width="48" height="48" alt="x_rv12"> | Van's RV-12, Tecnam Sierra, JMB VL3 | `RV12` `SIRA` `VL3` | 27 px |
| <img src="icons/x_lnc4.svg" width="48" height="48" alt="x_lnc4"> | Lancair IV / Evolution | `EVOT` `LNC2` `LNC4` | 28 px |
| <img src="icons/x_pa28.svg" width="48" height="48" alt="x_pa28"> | Piper PA-28 Cherokee/Warrior/Archer/Arrow | `P28A` `P28B` `P28R` `P28T` `P28U` `PA28` | 30 px |
| <img src="icons/x_tb20.svg" width="48" height="48" alt="x_tb20"> | Socata Trinidad/Tobago | `TB10` `TB20` `TB21` `TB9` `TOBA` | 29 px |
| <img src="icons/x_dr40.svg" width="48" height="48" alt="x_dr40"> | Robin DR400 | `DR40` | 28 px |
| <img src="icons/x_g115.svg" width="48" height="48" alt="x_g115"> | Grob 115/120 | `G115` `G120` | 29 px |
| <img src="icons/x_aa5.svg" width="48" height="48" alt="x_aa5"> | Grumman Tiger/Traveler, Globe Swift | `AA1` `AA5` `GC1` | 29 px |
| <img src="icons/x_tbm9.svg" width="48" height="48" alt="x_tbm9"> | Daher TBM 700/850/900, Epic | `EPIC` `TBM7` `TBM8` `TBM9` | 31 px |
| <img src="icons/x_kodi.svg" width="48" height="48" alt="x_kodi"> | Quest Kodiak | `KODI` | 32 px |
| <img src="icons/x_pc6.svg" width="48" height="48" alt="x_pc6"> | Pilatus Porter | `PC6P` `PC6T` | 34 px |
| <img src="icons/x_dhc2.svg" width="48" height="48" alt="x_dhc2"> | DHC-2 Beaver | `DHC2` | 33 px |
| <img src="icons/x_dhc3.svg" width="48" height="48" alt="x_dhc3"> | DHC-3 Otter | `DH3T` `DHC3` | 35 px |
| <img src="icons/x_c208.svg" width="48" height="48" alt="x_c208"> | Cessna Caravan | `C208` | 34 px |
| <img src="icons/x_pc12.svg" width="48" height="48" alt="x_pc12"> | Pilatus PC-12 | `PC12` | 34 px |
| <img src="icons/x_at8t.svg" width="48" height="48" alt="x_at8t"> | Air Tractor (crop duster) | `AT01` `AT3T` `AT4T` `AT5T` `AT6T` `AT8T` | 33 px |
| <img src="icons/x_pa25.svg" width="48" height="48" alt="x_pa25"> | Piper Pawnee | `PA25` `PA36` | 30 px |
| <img src="icons/x_la4.svg" width="48" height="48" alt="x_la4"> | Lake Buccaneer (pusher amphibian) | `LA25` `LA4` | 30 px |
| <img src="icons/x_icona5.svg" width="48" height="48" alt="x_icona5"> | ICON A5 / SeaRey | `ICON` `SRAY` | 30 px |
| <img src="icons/x_sling.svg" width="48" height="48" alt="x_sling"> | Sling 2/4/TSi | `SLG2` `SLG4` `SLGT` | 28 px |

## Light aircraft: twins and utility

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_pa23.svg" width="48" height="48" alt="x_pa23"> | Piper Aztec/Apache | `PA23` | 30 px |
| <img src="icons/x_pa34.svg" width="48" height="48" alt="x_pa34"> | Piper Seneca | `PA34` | 31 px |
| <img src="icons/x_pa44.svg" width="48" height="48" alt="x_pa44"> | Piper Seminole | `PA44` | 31 px |
| <img src="icons/x_be58.svg" width="48" height="48" alt="x_be58"> | Beech Baron | `B58T` `BE55` `BE58` `BE95` | 30 px |
| <img src="icons/x_be76.svg" width="48" height="48" alt="x_be76"> | Beech Duchess | `BE76` | 30 px |
| <img src="icons/x_c310.svg" width="48" height="48" alt="x_c310"> | Cessna 310/320 | `C303` `C310` `C320` | 30 px |
| <img src="icons/x_c340.svg" width="48" height="48" alt="x_c340"> | Cessna 340/335 | `C335` `C340` | 30 px |
| <img src="icons/x_c414.svg" width="48" height="48" alt="x_c414"> | Cessna 401/402/414 | `C401` `C402` `C411` `C414` | 32 px |
| <img src="icons/x_c421.svg" width="48" height="48" alt="x_c421"> | Cessna 421/404 | `C404` `C421` | 31 px |
| <img src="icons/x_da42.svg" width="48" height="48" alt="x_da42"> | Diamond DA42 | `DA42` | 32 px |
| <img src="icons/x_da62.svg" width="48" height="48" alt="x_da62"> | Diamond DA62 | `DA62` | 33 px |
| <img src="icons/x_p68.svg" width="48" height="48" alt="x_p68"> | Vulcanair/Partenavia P68 | `P68` | 31 px |
| <img src="icons/x_pa31.svg" width="48" height="48" alt="x_pa31"> | Piper Navajo/Chieftain | `PA31` `PA3T` | 31 px |
| <img src="icons/x_aest.svg" width="48" height="48" alt="x_aest"> | Piper Aerostar | `AEST` | 30 px |
| <img src="icons/x_b350.svg" width="48" height="48" alt="x_b350"> | King Air 350 | `B350` `BE30` | 35 px |
| <img src="icons/x_be9l.svg" width="48" height="48" alt="x_be9l"> | King Air 90/100 | `BE10` `BE9L` `BE9T` | 33 px |
| <img src="icons/x_sw4.svg" width="48" height="48" alt="x_sw4"> | Fairchild Metro/Merlin | `SW2` `SW3` `SW4` | 35 px |
| <img src="icons/x_js31.svg" width="48" height="48" alt="x_js31"> | Jetstream 31/32 | `JS31` `JS32` | 34 px |
| <img src="icons/x_mu2.svg" width="48" height="48" alt="x_mu2"> | Mitsubishi MU-2 | `MU2` | 31 px |
| <img src="icons/x_c441.svg" width="48" height="48" alt="x_c441"> | Cessna Conquest | `C425` `C441` | 33 px |
| <img src="icons/x_pay3.svg" width="48" height="48" alt="x_pay3"> | Piper Cheyenne | `PAY1` `PAY2` `PAY3` `PAY4` | 33 px |
| <img src="icons/x_p180.svg" width="48" height="48" alt="x_p180"> | Piaggio Avanti | `P180` | 33 px |
| <img src="icons/x_c337.svg" width="48" height="48" alt="x_c337"> | Cessna Skymaster (push-pull) | `C337` | 30 px |
| <img src="icons/x_l410.svg" width="48" height="48" alt="x_l410"> | Let 410 | `L410` | 36 px |
| <img src="icons/x_bn2.svg" width="48" height="48" alt="x_bn2"> | Britten-Norman Islander | `BN2P` `BN2T` | 33 px |
| <img src="icons/x_d228.svg" width="48" height="48" alt="x_d228"> | Dornier 228 | `D228` | 34 px |
| <img src="icons/x_c212.svg" width="48" height="48" alt="x_c212"> | CASA C-212 | `C212` | 36 px |
| <img src="icons/x_sc7.svg" width="48" height="48" alt="x_sc7"> | Shorts Skyvan / 330 / 360 | `SC7` `SH33` `SH36` | 36 px |
| <img src="icons/x_be20.svg" width="48" height="48" alt="x_be20"> | King Air 200/250/260 | `BE20` | 34 px |
| <img src="icons/x_p06t.svg" width="48" height="48" alt="x_p06t"> | Tecnam P2006T | `P06T` | 30 px |

## Classic airliners and transports

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_dc3.svg" width="48" height="48" alt="x_dc3"> | Douglas DC-3 / C-47 | `C47` `DC3` `DC3T` | 40 px |
| <img src="icons/x_dc4.svg" width="48" height="48" alt="x_dc4"> | Douglas DC-4 / C-54 | `DC4` | 43 px |
| <img src="icons/x_dc6.svg" width="48" height="48" alt="x_dc6"> | Douglas DC-6/DC-7 | `DC6` `DC7` | 43 px |
| <img src="icons/x_c46.svg" width="48" height="48" alt="x_c46"> | Curtiss C-46 Commando | `C46` | 42 px |
| <img src="icons/x_coni.svg" width="48" height="48" alt="x_coni"> | Lockheed Constellation | `CONI` `L049` `L1049` `L749` | 43 px |
| <img src="icons/x_l188.svg" width="48" height="48" alt="x_l188"> | Lockheed Electra | `L188` | 41 px |
| <img src="icons/x_visc.svg" width="48" height="48" alt="x_visc"> | Vickers Viscount | `VISC` | 40 px |
| <img src="icons/x_ju52.svg" width="48" height="48" alt="x_ju52"> | Junkers Ju 52 (trimotor) | `JU52` | 40 px |
| <img src="icons/x_an2.svg" width="48" height="48" alt="x_an2"> | Antonov An-2 (biplane) | `AN2` | 35 px |
| <img src="icons/x_be18.svg" width="48" height="48" alt="x_be18"> | Beech 18 (twin tail) | `BE18` | 33 px |
| <img src="icons/x_be17.svg" width="48" height="48" alt="x_be17"> | Beech Staggerwing (biplane) | `BE17` | 29 px |
| <img src="icons/x_g21.svg" width="48" height="48" alt="x_g21"> | Grumman Goose | `G21` `G44` | 33 px |
| <img src="icons/x_pby.svg" width="48" height="48" alt="x_pby"> | Consolidated Catalina | `CAT` `PBY` | 41 px |
| <img src="icons/x_dhc6.svg" width="48" height="48" alt="x_dhc6"> | DHC-6 Twin Otter | `DHC6` | 36 px |
| <img src="icons/x_cl415.svg" width="48" height="48" alt="x_cl415"> | Canadair CL-215/415 | `CL2P` `CL2T` `CL4T` | 40 px |

## Warbirds and classic fighters

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_p51.svg" width="48" height="48" alt="x_p51"> | P-51 Mustang | `P51` | 30 px |
| <img src="icons/x_spit.svg" width="48" height="48" alt="x_spit"> | Supermarine Spitfire | `SPIT` | 30 px |
| <img src="icons/x_huri.svg" width="48" height="48" alt="x_huri"> | Hawker Hurricane | `HURI` | 31 px |
| <img src="icons/x_me09.svg" width="48" height="48" alt="x_me09"> | Messerschmitt Bf 109 | `ME09` | 29 px |
| <img src="icons/x_fw90.svg" width="48" height="48" alt="x_fw90"> | Focke-Wulf Fw 190 | `FW90` | 30 px |
| <img src="icons/x_zero.svg" width="48" height="48" alt="x_zero"> | Mitsubishi Zero | `A6M` `ZERO` | 31 px |
| <img src="icons/x_cors.svg" width="48" height="48" alt="x_cors"> | Vought F4U Corsair | `CORS` `F4U` | 31 px |
| <img src="icons/x_p47.svg" width="48" height="48" alt="x_p47"> | P-47 Thunderbolt | `P47` | 31 px |
| <img src="icons/x_p40.svg" width="48" height="48" alt="x_p40"> | P-40 Warhawk | `P40` | 30 px |
| <img src="icons/x_p38.svg" width="48" height="48" alt="x_p38"> | P-38 Lightning (twin boom) | `P38` | 34 px |
| <img src="icons/x_b17.svg" width="48" height="48" alt="x_b17"> | B-17 Flying Fortress | `B17` | 41 px |
| <img src="icons/x_b24.svg" width="48" height="48" alt="x_b24"> | B-24 Liberator | `B24` | 42 px |
| <img src="icons/x_b25.svg" width="48" height="48" alt="x_b25"> | B-25 Mitchell | `B25` | 36 px |
| <img src="icons/x_b29.svg" width="48" height="48" alt="x_b29"> | B-29 Superfortress | `B29` | 45 px |
| <img src="icons/x_lanc.svg" width="48" height="48" alt="x_lanc"> | Avro Lancaster | `LANC` | 41 px |
| <img src="icons/x_mosq.svg" width="48" height="48" alt="x_mosq"> | de Havilland Mosquito | `MOSQ` | 34 px |
| <img src="icons/x_t6.svg" width="48" height="48" alt="x_t6"> | North American T-6 Texan/Harvard | `T6` | 31 px |
| <img src="icons/x_t28.svg" width="48" height="48" alt="x_t28"> | North American T-28 Trojan | `T28` | 31 px |
| <img src="icons/x_stearman.svg" width="48" height="48" alt="x_stearman"> | Boeing Stearman (biplane) | `ST75` | 29 px |
| <img src="icons/x_dh82.svg" width="48" height="48" alt="x_dh82"> | de Havilland Tiger Moth (biplane) | `DH82` | 28 px |
| <img src="icons/x_pitts.svg" width="48" height="48" alt="x_pitts"> | Pitts Special (biplane) | `PTS1` `PTS2` | 25 px |
| <img src="icons/x_yk52.svg" width="48" height="48" alt="x_yk52"> | Yak-52 / Yak-18 | `YK18` `YK52` | 29 px |
| <img src="icons/x_a1sky.svg" width="48" height="48" alt="x_a1sky"> | Douglas A-1 Skyraider | `A1` | 33 px |

## Experimental and aerobatic

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_lgez.svg" width="48" height="48" alt="x_lgez"> | Rutan Long-EZ / VariEze / Cozy | `COZY` `LGEZ` `VEZE` | 27 px |
| <img src="icons/x_velo.svg" width="48" height="48" alt="x_velo"> | Velocity | `VELO` | 28 px |
| <img src="icons/x_e300.svg" width="48" height="48" alt="x_e300"> | Extra 300/330 | `E200` `E300` `E330` | 27 px |
| <img src="icons/x_su26.svg" width="48" height="48" alt="x_su26"> | Sukhoi Su-26/29/31 | `SU26` `SU29` `SU31` | 27 px |
| <img src="icons/x_cp23.svg" width="48" height="48" alt="x_cp23"> | CAP 10/232 | `CP10` `CP23` | 27 px |
| <img src="icons/x_kitfox.svg" width="48" height="48" alt="x_kitfox"> | Kitfox / Zenith / light sport high wing | `C42` `CH60` `CH70` `ECHO` `KFOX` `P92` `SAVG` | 29 px |
| <img src="icons/x_xcub.svg" width="48" height="48" alt="x_xcub"> | CubCrafters Carbon/XCub | `CC11` `CC18` `CC19` `XCUB` | 29 px |
| <img src="icons/x_glasair.svg" width="48" height="48" alt="x_glasair"> | Glasair | `GLAS` `GLST` | 27 px |

## Military jets

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_f86.svg" width="48" height="48" alt="x_f86"> | F-86 Sabre | `F86` | 30 px |
| <img src="icons/x_mg15.svg" width="48" height="48" alt="x_mg15"> | MiG-15/17 | `MG15` `MG17` | 29 px |
| <img src="icons/x_mg21.svg" width="48" height="48" alt="x_mg21"> | MiG-21 | `MG21` | 33 px |
| <img src="icons/x_f104.svg" width="48" height="48" alt="x_f104"> | F-104 Starfighter | `F104` | 34 px |
| <img src="icons/x_vamp.svg" width="48" height="48" alt="x_vamp"> | de Havilland Vampire (twin boom) | `VAMP` | 30 px |
| <img src="icons/x_hunt.svg" width="48" height="48" alt="x_hunt"> | Hawker Hunter | `HUNT` | 32 px |
| <img src="icons/x_l39.svg" width="48" height="48" alt="x_l39"> | Aero L-39/L-159 | `L159` `L39` | 31 px |
| <img src="icons/x_a4.svg" width="48" height="48" alt="x_a4"> | A-4 Skyhawk | `A4` | 31 px |
| <img src="icons/x_f4.svg" width="48" height="48" alt="x_f4"> | F-4 Phantom II | `F4` | 35 px |
| <img src="icons/x_f14.svg" width="48" height="48" alt="x_f14"> | F-14 Tomcat | `F14` | 36 px |
| <img src="icons/x_f15.svg" width="48" height="48" alt="x_f15"> | F-15 Eagle | `F15` | 36 px |
| <img src="icons/x_f18.svg" width="48" height="48" alt="x_f18"> | F/A-18 Hornet / Super Hornet | `EA18` `F18` `F18S` | 35 px |
| <img src="icons/x_f22.svg" width="48" height="48" alt="x_f22"> | F-22 Raptor | `F22` | 35 px |
| <img src="icons/x_f35.svg" width="48" height="48" alt="x_f35"> | F-35 Lightning II | `F35` | 33 px |
| <img src="icons/x_su27.svg" width="48" height="48" alt="x_su27"> | Sukhoi Su-27/30/35 | `J11` `J15` `SU27` `SU30` `SU33` `SU35` | 37 px |
| <img src="icons/x_mg29.svg" width="48" height="48" alt="x_mg29"> | MiG-29 | `MG29` `MG35` | 34 px |
| <img src="icons/x_tornado.svg" width="48" height="48" alt="x_tornado"> | Panavia Tornado | `TOR` | 34 px |
| <img src="icons/x_jaguar.svg" width="48" height="48" alt="x_jaguar"> | SEPECAT Jaguar | `JAGR` | 34 px |
| <img src="icons/x_mir2.svg" width="48" height="48" alt="x_mir2"> | Mirage 2000 / Mirage III | `M2000` `MIR2` `MIR3` | 33 px |
| <img src="icons/x_draken.svg" width="48" height="48" alt="x_draken"> | Saab Draken | `DRAK` | 33 px |
| <img src="icons/x_viggen.svg" width="48" height="48" alt="x_viggen"> | Saab Viggen | `VIGN` | 34 px |
| <img src="icons/x_rafale.svg" width="48" height="48" alt="x_rafale"> | Dassault Rafale | `RFAL` | 33 px |
| <img src="icons/x_gripen.svg" width="48" height="48" alt="x_gripen"> | Saab Gripen | `GRIF` `JAS3` | 32 px |
| <img src="icons/x_eufi.svg" width="48" height="48" alt="x_eufi"> | Eurofighter Typhoon | `EUFI` | 34 px |
| <img src="icons/x_j10.svg" width="48" height="48" alt="x_j10"> | Chengdu J-10 | `J10` | 34 px |
| <img src="icons/x_f16.svg" width="48" height="48" alt="x_f16"> | F-16 Fighting Falcon | `F16` `F2` | 33 px |
| <img src="icons/x_f5.svg" width="48" height="48" alt="x_f5"> | F-5 / T-38 | `F5` `T38` | 33 px |
| <img src="icons/x_hawk.svg" width="48" height="48" alt="x_hawk"> | BAE Hawk / T-45 | `HAWK` `T45` | 31 px |
| <img src="icons/x_ajet.svg" width="48" height="48" alt="x_ajet"> | Dassault Alpha Jet | `AJET` | 31 px |
| <img src="icons/x_m346.svg" width="48" height="48" alt="x_m346"> | M-346 / Yak-130 | `M346` `YK13` | 30 px |
| <img src="icons/x_t7.svg" width="48" height="48" alt="x_t7"> | Boeing T-7 Red Hawk | `T7` | 32 px |
| <img src="icons/x_a10.svg" width="48" height="48" alt="x_a10"> | A-10 Thunderbolt II | `A10` | 34 px |
| <img src="icons/x_av8b.svg" width="48" height="48" alt="x_av8b"> | Harrier | `AV8B` `HAR` `HAR2` | 32 px |
| <img src="icons/x_f117.svg" width="48" height="48" alt="x_f117"> | F-117 Nighthawk | `F117` | 36 px |
| <img src="icons/x_sr71.svg" width="48" height="48" alt="x_sr71"> | SR-71 Blackbird | `SR71` | 42 px |
| <img src="icons/x_u2.svg" width="48" height="48" alt="x_u2"> | Lockheed U-2 | `U2` | 41 px |

## Military transports, bombers, tankers and patrol

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_c5.svg" width="48" height="48" alt="x_c5"> | C-5 Galaxy | `C5` `C5M` | 53 px |
| <img src="icons/x_a400.svg" width="48" height="48" alt="x_a400"> | Airbus A400M | `A400` | 46 px |
| <img src="icons/x_p3.svg" width="48" height="48" alt="x_p3"> | P-3 Orion / L-188 | `P3` | 43 px |
| <img src="icons/x_c160.svg" width="48" height="48" alt="x_c160"> | Transall C-160 | `C160` | 44 px |
| <img src="icons/x_an124.svg" width="48" height="48" alt="x_an124"> | Antonov An-124 | `A124` | 53 px |
| <img src="icons/x_an225.svg" width="48" height="48" alt="x_an225"> | Antonov An-225 | `A225` | 56 px |
| <img src="icons/x_il76.svg" width="48" height="48" alt="x_il76"> | Ilyushin Il-76 | `A50` `IL76` | 47 px |
| <img src="icons/x_q4.svg" width="48" height="48" alt="x_q4"> | RQ-4 Global Hawk | `Q4` `RQ4` | 44 px |
| <img src="icons/x_mq9.svg" width="48" height="48" alt="x_mq9"> | MQ-9 Reaper / MQ-1 | `HRON` `MQ1` `MQ9` `Q1` `Q9` | 36 px |
| <img src="icons/x_kc135.svg" width="48" height="48" alt="x_kc135"> | KC-135 / Boeing 707 / E-6 | `B701` `B703` `B720` `C135` `E6` `K35E` `K35R` `K35T` `R135` | 45 px |
| <img src="icons/x_kc46.svg" width="48" height="48" alt="x_kc46"> | KC-46 Pegasus / Boeing 767-200 | `K46` `KC46` | 47 px |
| <img src="icons/x_c17.svg" width="48" height="48" alt="x_c17"> | Boeing C-17 Globemaster III | `C17` | 48 px |
| <img src="icons/x_c130.svg" width="48" height="48" alt="x_c130"> | Lockheed C-130 Hercules, An-12, Il-18 | `AN12` `C130` `C30J` `IL18` | 44 px |
| <img src="icons/x_c27j.svg" width="48" height="48" alt="x_c27j"> | Alenia C-27J Spartan | `C27J` | 40 px |

## Helicopters

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/x_r22.svg" width="48" height="48" alt="x_r22"> | Robinson R22 | `R22` | 28 px |
| <img src="icons/x_r44.svg" width="48" height="48" alt="x_r44"> | Robinson R44 | `R44` | 31 px |
| <img src="icons/x_r66.svg" width="48" height="48" alt="x_r66"> | Robinson R66 | `R66` | 31 px |
| <img src="icons/x_b06.svg" width="48" height="48" alt="x_b06"> | Bell 206 JetRanger / LongRanger | `B06` `B06T` | 31 px |
| <img src="icons/x_b505.svg" width="48" height="48" alt="x_b505"> | Bell 505 | `B505` | 31 px |
| <img src="icons/x_b47g.svg" width="48" height="48" alt="x_b47g"> | Bell 47 (bubble) | `B47G` | 32 px |
| <img src="icons/x_uh1.svg" width="48" height="48" alt="x_uh1"> | Bell UH-1 Huey / 204/205/212 | `B204` `B205` `B212` `UH1` | 34 px |
| <img src="icons/x_ah1.svg" width="48" height="48" alt="x_ah1"> | Bell AH-1 Cobra | `AH1` `H1` | 35 px |
| <img src="icons/x_b222.svg" width="48" height="48" alt="x_b222"> | Bell 222/230 | `B222` `B230` | 33 px |
| <img src="icons/x_b407.svg" width="48" height="48" alt="x_b407"> | Bell 407 | `B407` | 31 px |
| <img src="icons/x_b412.svg" width="48" height="48" alt="x_b412"> | Bell 412 | `B412` | 34 px |
| <img src="icons/x_b429.svg" width="48" height="48" alt="x_b429"> | Bell 427/429 | `B427` `B429` | 32 px |
| <img src="icons/x_b525.svg" width="48" height="48" alt="x_b525"> | Bell 525 | `B525` | 36 px |
| <img src="icons/x_h500.svg" width="48" height="48" alt="x_h500"> | MD 500 | `H500` `H520` `H530` | 29 px |
| <img src="icons/x_md900.svg" width="48" height="48" alt="x_md900"> | MD 900 Explorer (NOTAR) | `EXPL` `MD60` `MD90X` | 31 px |
| <img src="icons/x_as50.svg" width="48" height="48" alt="x_as50"> | Airbus AS350 / H125 | `AS50` `H125` | 31 px |
| <img src="icons/x_as55.svg" width="48" height="48" alt="x_as55"> | Airbus AS355 TwinStar | `AS55` | 31 px |
| <img src="icons/x_ec30.svg" width="48" height="48" alt="x_ec30"> | Airbus EC130 / H130 | `EC30` `H130` | 31 px |
| <img src="icons/x_ec20.svg" width="48" height="48" alt="x_ec20"> | Airbus EC120 Colibri | `EC20` | 30 px |
| <img src="icons/x_ec35.svg" width="48" height="48" alt="x_ec35"> | Airbus EC135 / H135 | `EC35` `H135` | 31 px |
| <img src="icons/x_ec45.svg" width="48" height="48" alt="x_ec45"> | Airbus H145 / BK117 | `BK17` `EC45` `H145` | 32 px |
| <img src="icons/x_as65.svg" width="48" height="48" alt="x_as65"> | Airbus Dauphin / H155 | `AS65` `EC55` `H155` | 32 px |
| <img src="icons/x_h160.svg" width="48" height="48" alt="x_h160"> | Airbus H160 | `H160` | 33 px |
| <img src="icons/x_ec75.svg" width="48" height="48" alt="x_ec75"> | Airbus H175 | `EC75` `H175` | 35 px |
| <img src="icons/x_ec25.svg" width="48" height="48" alt="x_ec25"> | Airbus H225 / Super Puma | `AS32` `AS3B` `EC25` `H215` `H225` | 36 px |
| <img src="icons/x_gazl.svg" width="48" height="48" alt="x_gazl"> | Aerospatiale Gazelle | `GAZL` | 31 px |
| <img src="icons/x_alo3.svg" width="48" height="48" alt="x_alo3"> | Alouette II/III, Lama | `ALO2` `ALO3` `LAMA` | 31 px |
| <img src="icons/x_a109.svg" width="48" height="48" alt="x_a109"> | Leonardo A109 / A119 | `A109` `A119` | 32 px |
| <img src="icons/x_a139.svg" width="48" height="48" alt="x_a139"> | Leonardo AW139 | `A139` | 34 px |
| <img src="icons/x_a169.svg" width="48" height="48" alt="x_a169"> | Leonardo AW169 | `A169` | 33 px |
| <img src="icons/x_a189.svg" width="48" height="48" alt="x_a189"> | Leonardo AW189 / AW149 | `A149` `A189` | 35 px |
| <img src="icons/x_eh10.svg" width="48" height="48" alt="x_eh10"> | Leonardo AW101 / Merlin | `EH10` | 37 px |
| <img src="icons/x_s76.svg" width="48" height="48" alt="x_s76"> | Sikorsky S-76 | `S76` | 34 px |
| <img src="icons/x_s92.svg" width="48" height="48" alt="x_s92"> | Sikorsky S-92 / H-92 | `H92` `S92` | 36 px |
| <img src="icons/x_h60.svg" width="48" height="48" alt="x_h60"> | Sikorsky UH-60 Black Hawk / S-70 | `H60` `MH60` `S70` `UH60` | 36 px |
| <img src="icons/x_h53.svg" width="48" height="48" alt="x_h53"> | Sikorsky CH-53 | `CH53` `H53` `H53S` | 41 px |
| <img src="icons/x_s61.svg" width="48" height="48" alt="x_s61"> | Sikorsky S-61 / Sea King | `H3` `S61` | 37 px |
| <img src="icons/x_h47.svg" width="48" height="48" alt="x_h47"> | Boeing CH-47 Chinook | `CH47` `H47` | 41 px |
| <img src="icons/x_h46.svg" width="48" height="48" alt="x_h46"> | Boeing CH-46 Sea Knight | `H46` | 39 px |
| <img src="icons/x_h64.svg" width="48" height="48" alt="x_h64"> | Boeing AH-64 Apache | `AH64` `H64` | 35 px |
| <img src="icons/x_tigr.svg" width="48" height="48" alt="x_tigr"> | Airbus Tiger | `TIGR` | 33 px |
| <img src="icons/x_a129.svg" width="48" height="48" alt="x_a129"> | Leonardo A129 Mangusta | `A129` | 32 px |
| <img src="icons/x_mi8.svg" width="48" height="48" alt="x_mi8"> | Mil Mi-8/17 | `MI14` `MI17` `MI8` | 39 px |
| <img src="icons/x_mi24.svg" width="48" height="48" alt="x_mi24"> | Mil Mi-24/35 Hind | `MI24` `MI35` | 37 px |
| <img src="icons/x_mi26.svg" width="48" height="48" alt="x_mi26"> | Mil Mi-26 | `MI26` | 44 px |
| <img src="icons/x_mi28.svg" width="48" height="48" alt="x_mi28"> | Mil Mi-28 | `MI28` | 37 px |
| <img src="icons/x_ka52.svg" width="48" height="48" alt="x_ka52"> | Kamov Ka-50/52 | `KA50` `KA52` | 33 px |
| <img src="icons/x_ka32.svg" width="48" height="48" alt="x_ka32"> | Kamov Ka-27/32 | `KA26` `KA27` `KA32` | 34 px |
| <img src="icons/x_nh90.svg" width="48" height="48" alt="x_nh90"> | NHIndustries NH90 | `NH90` | 36 px |
| <img src="icons/x_lynx.svg" width="48" height="48" alt="x_lynx"> | Westland Lynx / AW159 | `A159` `LYNX` | 33 px |
| <img src="icons/x_en28.svg" width="48" height="48" alt="x_en28"> | Enstrom 280/480 | `EN28` `EN48` | 29 px |
| <img src="icons/x_h269.svg" width="48" height="48" alt="x_h269"> | Schweizer 300 / Hughes 269 | `H269` `S300` | 29 px |
| <img src="icons/x_g2ca.svg" width="48" height="48" alt="x_g2ca"> | Guimbal Cabri G2 | `G2CA` | 27 px |

## Families (shared shapes)

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/g_single_high.svg" width="48" height="48" alt="g_single_high"> | A1 light (unknown type): a high-wing single | *(by emitter category)* | 30 px |
| <img src="icons/g_single_high.svg" width="48" height="48" alt="g_single_high"> | Cessna 150-210 and other high-wing singles | `C162` `C175` | 30 px |
| <img src="icons/g_single_low.svg" width="48" height="48" alt="g_single_low"> | Piper PA-28/32, Cirrus, Diamond, Bonanza, Mooney, RV | `BE19` `BE23` `BE24` `COL3` `COL4` `CRUZ` `F260` | 30 px |
| <img src="icons/g_turboprop_single.svg" width="48" height="48" alt="g_turboprop_single"> | PC-12, TBM, PA-46, T-6, Super Tucano, Air Tractor | `E314` `PC21` `PC7` `PC9` `T34` `TEX2` | 34 px |
| <img src="icons/g_twin_piston.svg" width="48" height="48" alt="g_twin_piston"> | Baron, Seneca, Seminole, Cessna 310-421, DA42 | `BE60` `PA27` `PA30` | 32 px |
| <img src="icons/g_twin_turboprop.svg" width="48" height="48" alt="g_twin_turboprop"> | Beech 99, Metro, Jetstream, Conquest, MU-2, Cheyenne | `AC11` `AC90` `AC95` `BE99` `E121` `F406` `PA42` | 36 px |
| <img src="icons/g_twin_high.svg" width="48" height="48" alt="g_twin_high"> | Twin Otter, Islander, Do 228, Let 410, CASA 212, Skyvan, P68 | `AN28` `DO28` `M28` | 38 px |
| <img src="icons/g_awacs.svg" width="48" height="48" alt="g_awacs"> | AWACS: E-3, A-50, E-767 | `E3CF` `E3TF` `E767` | 50 px |
| <img src="icons/g_hawkeye.svg" width="48" height="48" alt="g_hawkeye"> | E-2 Hawkeye, C-2 | `C2` `E2` | 40 px |
| <img src="icons/g_b52.svg" width="48" height="48" alt="g_b52"> | B-52 | `B52` | 56 px |
| <img src="icons/g_b1.svg" width="48" height="48" alt="g_b1"> | B-1 | `B1` | 50 px |
| <img src="icons/g_b2.svg" width="48" height="48" alt="g_b2"> | B-2 | `B2` | 54 px |
| <img src="icons/g_fighter_single.svg" width="48" height="48" alt="g_fighter_single"> | single-fin fighters: F-16, F-4, F-5, Tornado, A-4 | `AMX` `JF17` `SU25` | 34 px |
| <img src="icons/g_fighter_twin.svg" width="48" height="48" alt="g_fighter_twin"> | twin-fin fighters: F-15, F/A-18, F-22, F-35, F-14, Su-27 family, MiG-29 | `J20` `MG31` `SU34` `SU57` | 38 px |
| <img src="icons/g_fighter_delta.svg" width="48" height="48" alt="g_fighter_delta"> | delta fighters: Eurofighter, Rafale, Gripen, Mirage 2000, J-10 | `KFIR` | 36 px |
| <img src="icons/g_jet_trainer.svg" width="48" height="48" alt="g_jet_trainer"> | jet trainers: T-38, Hawk, L-39, M-346, Alpha Jet, T-45, T-7 | `K8` `T50` | 32 px |
| <img src="icons/g_heli_4.svg" width="48" height="48" alt="g_heli_4"> | four-blade helicopters: H135, H145, Bell 407/429/412, A109, S-76, Dauphin | `B430` | 36 px |

## Other ADS-B Radar icons

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/dh8a.svg" width="48" height="48" alt="dh8a"> | regional turboprops: Dash 8, ATR, Saab, Do 328 | `IL14` | 40 px |
| <img src="icons/heavyfreighter.svg" width="48" height="48" alt="heavyfreighter"> | heavy jet freighters: C-17, C-5, An-124, Il-76 | `AN22` | 54 px |
| <img src="icons/tiltrotorcraft.svg" width="48" height="48" alt="tiltrotorcraft"> | V-22, AW609 | `B609` `V22` | 40 px |

## Size classes (when the type is unknown)

| Icon | Aircraft | Type codes | Size |
|:---:|---|---|---:|
| <img src="icons/a0.svg" width="48" height="48" alt="a0"> | A0 / unknown | *(by emitter category)* | 40 px |
| <img src="icons/a2.svg" width="48" height="48" alt="a2"> | A2 small | *(by emitter category)* | 34 px |
| <img src="icons/a3.svg" width="48" height="48" alt="a3"> | A3 large | *(by emitter category)* | 42 px |
| <img src="icons/a4.svg" width="48" height="48" alt="a4"> | A4 (757 class) | *(by emitter category)* | 46 px |
| <img src="icons/a5.svg" width="48" height="48" alt="a5"> | A5 heavy | *(by emitter category)* | 48 px |
| <img src="icons/a6.svg" width="48" height="48" alt="a6"> | A6 high performance | *(by emitter category)* | 36 px |
| <img src="icons/a7.svg" width="48" height="48" alt="a7"> | A7 rotorcraft (unknown type) | *(by emitter category)* | 38 px |
| <img src="icons/b0.svg" width="48" height="48" alt="b0"> | B6 unmanned | *(by emitter category)* | 22 px |
| <img src="icons/b1.svg" width="48" height="48" alt="b1"> | B1 glider | `AS21` `ASK21` `ASK3` `ASW2` `DG1T` `DIMO` `DISC` `DUOD` `GLID` `GLIM` `JANU` `LS8` `NIMB` `VENT` | 32 px |
| <img src="icons/b2.svg" width="48" height="48" alt="b2"> | B2 lighter than air | `BALL` `SHIP` | 30 px |
| <img src="icons/b3.svg" width="48" height="48" alt="b3"> | B3 parachutist | *(by emitter category)* | 20 px |
| <img src="icons/b4.svg" width="48" height="48" alt="b4"> | B4 ultralight | `PIAT` `ULAC` | 28 px |
| <img src="icons/c0.svg" width="48" height="48" alt="c0"> | C1-C7 surface vehicles; ground stations | `GND` `SERV` `TWR` | 18 px |
