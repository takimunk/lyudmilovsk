# The Rassvet's films: sources and licences

Written by `tools/films/fetch_films.py` from `catalog.py` and `films.lock.json`; do not edit by hand.

The films are not in git: `python3 tools/films/fetch_films.py` downloads them from Wikimedia Commons into
`unreal/Lyudmilovsk/Content/Films/` (ignored by git, staged as plain files). Each copy is used only when its Commons
page says Public domain; the script refuses any other licence.

## Why they are free

Every film was published before 1929, so it is in the public domain in the United States; every maker died in 1955
or earlier, so the term (life + 70) is over in the European Union, Russia and the United Kingdom, and in Japan both
the author term and the old 70-years-from-release term for films are over. **Exception: Mexico** (life + 100): the
Lumières (d. 1948, 1954), Méliès (d. 1938), Cohl (d. 1938), Chomón (d. 1929) and Porter (d. 1941) are not yet free
there, so a release that ships in Mexico must leave `Content/Films` out (the cinema then goes straight to the feature).

The lecturer's words (the subtitles over the films) are ours, CC BY 4.0, like the rest of the game's text.

## The films

| id | film | year | made by (died) | Commons copy | licence on the page | length |
|---|---|---|---|---|---|---|
| `lumiere_train` | The Arrival of a Train at La Ciotat | 1896 | Auguste and Louis Lumière (1954, 1948) | not fetched yet |  |  |
| `lumiere_factory` | Workers Leaving the Lumière Factory in Lyon | 1895 | Louis Lumière (1948) | not fetched yet |  |  |
| `lumiere_sprinkler` | The Sprinkler Sprinkled | 1895 | Louis Lumière (1948) | not fetched yet |  |  |
| `lumiere_baby` | Baby's Meal | 1895 | Louis Lumière (1948) | not fetched yet |  |  |
| `cohl_fantasmagorie` | Fantasmagorie | 1908 | Émile Cohl (1938) | not fetched yet |  |  |
| `melies_rubber_head` | The Man with the Rubber Head | 1901 | Georges Méliès (1938) | not fetched yet |  |  |
| `melies_moon` | A Trip to the Moon | 1902 | Georges Méliès (1938) | not fetched yet |  |  |
| `chomon_hotel` | The Electric Hotel | 1908 | Segundo de Chomón (1929) | not fetched yet |  |  |
| `porter_robbery` | The Great Train Robbery | 1903 | Edwin S. Porter (1941) | not fetched yet |  |  |

Authors and credit lines as Commons gives them are kept in `films.lock.json` (`artist`, `credit`).
