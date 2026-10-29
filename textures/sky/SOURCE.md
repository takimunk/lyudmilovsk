# textures/sky

`europa_2k.jpg` — Europa's surface for the moon in the sky (ALyudSky, `unreal/scripts/import_sky.py` imports it as
`/Game/Scene01/Sky/T_Europa`).

- Source: USGS Astrogeology, *Europa Voyager - Galileo SSI Global Mosaic 500m*,
  https://planetarymaps.usgs.gov/mosaic/Europa_Voyager_GalileoSSI_global_mosaic_500m.tif
  (19631 x 9816, simple cylindrical, 0..360 E left to right, north up).
- Licence: public domain (a U.S. Government work: NASA/JPL Galileo SSI and Voyager images, mosaic by USGS).
- Made by: resized to 2048 x 1024 (Lanczos), the no-data rows at the south pole filled with the median grey,
  contrast stretched (1% cut each end), JPEG quality 92 (`tools/sky/` has the shader that tints it ice and
  red-brown lineae).
