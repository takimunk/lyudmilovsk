"""Russian case forms of the townsfolk's given names (tools/town/names.json), for the lines of a talk that name somebody
(docs/town_talk.md "A talk"): nominative, genitive, dative, accusative, instrumental, prepositional.

Rules: a man's name ending in a consonant, -й or -ь declines; a name ending in -а / -я declines for both sexes unless a
vowel stands before the -а (Лихуа, Акосуа); a woman's name ending in a consonant or anything else (-о, -е, -и, -у) does not
change, nor does a man's name ending in a vowel other than -а / -я; a woman's name in -ь declines (Яэль, Яэли).
EXCEPT holds what the rules get wrong. Run this file to print the whole table for a read-through.
"""
VOWELS = "аеёиоуыэюя"
HUSH = "жшчщц"
VELAR = "гкх"

# name -> (gen, dat, acc, ins, pre)
EXCEPT = {
    "Пётр": ("Петра", "Петру", "Петра", "Петром", "Петре"),
    "Илья": ("Ильи", "Илье", "Илью", "Ильёй", "Илье"),
    # Chinese, Vietnamese and Korean one-syllable names are left as they are in everyday speech
    "Вэй": None, "Минь": None, "Дык": None, "Лан": None,
}


def decline(name, sex):
    """(nom, gen, dat, acc, ins, pre)"""
    if name in EXCEPT:
        e = EXCEPT[name]
        return (name,) + (e if e else (name,) * 5)
    last, stem = name[-1].lower(), name[:-1]
    if last == "а":
        if len(name) > 1 and name[-2].lower() in VOWELS:
            return (name,) * 6
        gen = stem + ("и" if stem[-1].lower() in VELAR + HUSH else "ы")
        ins = stem + ("ей" if stem[-1].lower() in HUSH else "ой")
        return (name, gen, stem + "е", stem + "у", ins, stem + "е")
    if last == "я":
        if stem[-1].lower() == "и":     # Лусия, Соломия, Прия
            return (name, stem + "и", stem + "и", stem + "ю", stem + "ей", stem + "и")
        return (name, stem + "и", stem + "е", stem + "ю", stem + "ей", stem + "е")
    if sex == "f":
        if last == "ь":
            return (name, stem + "и", stem + "и", name, name + "ю", stem + "и")
        return (name,) * 6
    if last == "й":
        pre = stem + ("и" if stem[-1].lower() == "и" else "е")       # Дмитрий: о Дмитрии
        return (name, stem + "я", stem + "ю", stem + "я", stem + "ем", pre)
    if last == "ь":
        return (name, stem + "я", stem + "ю", stem + "я", stem + "ем", stem + "е")
    if last in VOWELS:
        return (name,) * 6
    ins = name + ("ем" if last in HUSH else "ом")
    return (name, name + "а", name + "у", name + "а", ins, name + "е")


if __name__ == "__main__":
    import json
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    for g in json.load(open(os.path.join(here, "..", "town", "names.json"), encoding="utf-8"))["given"]:
        print(g["sex"], " | ".join(decline(g["ru"], g["sex"])))
