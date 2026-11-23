# Cobra

Un jeu en 3D isométrique façon Landstalker pour Mega Drive seule.
Étape 0 : la zone de démo affichée par le VDP, le héros (silhouette
provisoire) et sa vraie ombre en mode ombre et surbrillance. Fiche du
projet : [ARCHITECTURE.md](ARCHITECTURE.md) ; historique : [VERSION.md](VERSION.md).

## La ROM

`rom/cobra.bin` (256 Ko) est dans le dépôt, déjà assemblée : elle se lance
dans n'importe quel émulateur Mega Drive (BlastEm, Genesis Plus GX…) ou
sur une console avec une cartouche flash.

- Croix : déplacer le héros (haut = vers le haut de l'écran, droite = vers
  la droite ; les diagonales suivent les axes du monde).
- A, B ou C : sauter.

## Construire

Il faut CMake et un compilateur C (clang sur Mac). Pour réassembler la ROM,
un assembleur compatible asm68k : `asm68k` s'il est dans le PATH, sinon
`make asm` construit clownassembler (C++20, licence AGPL) dans `build/`.

```
make            # les outils et la conversion de la zone (build/data/)
make rom        # rom/cobra.bin, depuis rom/cobra.asm et build/data/
make test       # les tests (ASan et UBSan)
make sim        # la ROM sur le banc, avec demo/promenade.txt
```

`make data` produit aussi `build/data/attendu.bmp` (l'image de la zone telle
que le VDP doit l'afficher), `build/data/plans.bmp` (le plan A en jaune, les
positions du héros en vert : priorité basse, bleu : haute, rouge : masquée)
et `build/data/mesures.txt`.

## Modifier la zone

- `demo/zone/demo.vox` s'ouvre dans MagicaVoxel ; le numéro de couleur
  d'un voxel est son type de cube (`demo/zone/cubes.txt`). Pour tenir dans
  un plan de 64 x 64 tiles : W + D <= 32, et D impair.
- `tools/demozone.py` réécrit la zone de démo ; `tools/cubes2bmp.py`
  redessine les cubes et le héros provisoires (Pillow).

## Vérifier

`tests/run_tests.sh` (ou `make test`). Les tests sont compilés avec
AddressSanitizer et UBSan (`-DCOBRA_SANITIZE=OFF` pour s'en passer).

1. `test_scene` : l'image de la zone (ordre du peintre) contre un lancer de
   rayons, pixel par pixel (démo et 100 zones au hasard) ; « ce voxel
   est-il devant le héros ? » contre le rayon ; le plan A et les priorités
   contre leur définition ; les tiles redonnent l'image.
2. `rom/cobra.bin` est réassemblée (si un assembleur est là) et doit être
   identique.
3. `test_rom` : la ROM sur le banc (Musashi + modèle du VDP) : elle
   démarre ; le décor affiché est l'image attendue, au pixel près, tout au
   long d'une promenade ; l'ombre assombrit de moitié ; à 1 500 positions
   quelconques, le héros et son ombre sont cachés exactement par ce qui est
   devant eux ; pendant 120 sauts, l'ombre aussi (le héros en l'air est
   compté) ; 20 sprites par ligne au plus ; les cycles par image.

## Fichiers

| | |
|---|---|
| `iso/iso.h` | le treillis de triangles (repris de DDS 32) |
| `tools/` | `cobraconv` (vox.c, scene.c, planes.c, tiles.c), `romsum`, scripts Python |
| `rom/cobra.asm` | la ROM, en asm68k |
| `sim/` | le banc : `md.c` (machine), `vdp.c` (modèle du VDP), `cobrasim.c` |
| `tests/` | les tests |
| `demo/` | la zone, les cubes, le héros provisoires, la promenade de `make sim` |
| `third_party/musashi/` | Musashi (licence MIT), tel quel |
