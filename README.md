# Милый моему сердцу Людмиловск

Игра от первого лица. Стек: Unreal Engine 5 + Blender.

## Структура

- `blender/scripts/` — скрипты, которые собирают сцены в Blender (запускаются headless).
- `blender/` — собранные `.blend` файлы.
- `blender/export/` — экспорт для Unreal (`.glb`).
- `textures/` — сгенерированные текстуры (экран ОС героя и т.п.).
- `renders/` — контрольные рендеры.
- `unreal/Lyudmilovsk/` — проект Unreal Engine 5.8 (шаблон First Person).
- `unreal/scripts/` — скрипты, которые собирают уровни Unreal из `.glb`.

## Сцена 1: Коридор-пирамида

Длинный коридор с ступенчатыми (корбельными) стенами, как Большая галерея пирамиды.
Вдоль левой стены бегущая дорожка, вдоль правой длинный стол со стульями и самыми
разными компьютерами и консолями: CRT с Sega, телевизор с PS2, игровой ПК с подсветкой,
бежевый ПК девяностых, ZX-подобный компьютер, ноутбук, iMac G3, современная консоль.
В конце коридора включённый компьютер героя с простой ОС. Игрок просыпается от первого
лица в темноте, глядя на этот экран. Выхода пока нет.

Пересобрать сцену:

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b -P blender/scripts/build_scene01_corridor.py
```

## Unreal Engine

Проект: `unreal/Lyudmilovsk/Lyudmilovsk.uproject` (UE 5.8, Blueprint, без C++).
Стартовый уровень: `/Game/Scene01/Lvl_Scene01`, игрок появляется у стула героя
лицом к включённому экрану. Управление шаблонное: WASD, мышь, пробел.

Пересобрать уровень из `.glb` (редактор должен быть закрыт):

```bash
unreal/scripts/build_scene01.sh
```

### Unreal MCP

В проекте включён встроенный плагин Epic «Unreal MCP» (ModelContextProtocol)
и все тулсеты. Сервер стартует вместе с редактором: `http://localhost:8000/mcp`.
`.mcp.json` в корне репозитория подключает его к Claude Code автоматически.

## Git и LFS

Бинарные файлы (`.uasset`, `.umap`, `.blend`, `.glb`, картинки, звук, видео)
хранятся через Git LFS, правила в `.gitattributes`. `.gitignore` и LFS не мешают
друг другу: `.gitignore` решает, попадёт ли файл в репозиторий вообще, а
`.gitattributes` решает, как он хранится. Перед клонированием нужен `git lfs install`.

## Лицензии

- Код и скрипты: MIT, см. `LICENSE`.
- Собственные ассеты (модели, текстуры, рендеры, уровни): CC BY 4.0, см. `LICENSE-ASSETS`.
- Контент из шаблонов Epic (`unreal/Lyudmilovsk/Content/Characters`, `Input`,
  `LevelPrototyping`, `FirstPerson`) остаётся под лицензией Unreal Engine EULA
  и под эти лицензии не подпадает.
