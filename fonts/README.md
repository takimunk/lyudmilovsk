# Fonts

The hero's handwriting in the diary (docs/diary.md), and the train's print (docs/railway.md). All are under the SIL Open Font License 1.1 (the OFL-*.txt beside them);
unreal/scripts/import_diary_assets.py imports them as font faces into /Game/UI/Diary/Fonts.

| File | Font | Used for | Source |
|---|---|---|---|
| Caveat-Regular.ttf, Caveat-Bold.ttf | Caveat (Impallari Type), static instances at weight 400 and 700 of the variable font, made with fontTools.varLib.instancer | Russian and English | https://github.com/google/fonts/tree/main/ofl/caveat |
| Yomogi-Regular.ttf | Yomogi (Satsuyako) | Japanese | https://github.com/google/fonts/tree/main/ofl/yomogi |
| DelaGothicOne-Regular.ttf | Dela Gothic One (artakana) | the train's posters and notices: headlines (tools/train_ads.py) | https://github.com/google/fonts/tree/main/ofl/delagothicone |
| MPLUS1p-Bold.ttf, MPLUS1p-Regular.ttf | M PLUS 1p (M+ Fonts Project) | the train's posters and notices: text | https://github.com/google/fonts/tree/main/ofl/mplus1p |
