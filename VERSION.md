# Cobra : historique des versions

---

Étape 0 : la zone affichée par le VDP, le héros et sa vraie ombre

**Ce que fait cette version** :
- **Nouveau dépôt.** De DDS 32, on reprend le monde de voxels (MagicaVoxel,
  `cubes.txt`, images de cube 32 x 32), le treillis de triangles
  (`iso/iso.h`) et la lecture des BMP. Le reste est neuf. La fiche du
  projet est dans `ARCHITECTURE.md`.
- **Zone de démo** de 17 x 15 x 8 cases, dans des teintes gris et bleu
  foncé : un sol de dalles, un canal et un pont, une arche, une colonnade
  (deux rangées de colonnes), un plateau, un mur et des caisses. Son
  image fait 512 x 384 pixels et tient dans un plan de 64 x 64 tiles. Le
  héros est une silhouette provisoire, rouge avec des bottes noires.
- **`cobraconv`**, notre « Mirage Type III » :
  - il dessine la zone pixel par pixel (ordre du peintre) ;
  - il range chaque pixel de décor dans le plan A s'il peut cacher le
    héros à l'une de ses positions possibles (tous les 4 unités, posé ou
    un niveau au-dessus), sinon dans le plan B ;
  - il découpe les deux plans en tiles 8 x 8, sans doublon (retournements
    compris), sur deux lignes de palette de décor (11 couleurs chacune,
    plus 4 réservées au rayon delta) ;
  - il écrit la priorité du héros à chaque position (`prio.bin`) : basse,
    haute, ou conflit (on prend alors celle qui fait le moins de pixels
    faux) ;
  - il produit l'image attendue, une image des plans et les mesures.
- **La ROM** (`rom/cobra.asm`, asm68k ; `rom/cobra.bin` est dans le dépôt,
  déjà assemblée) :
  - mode 320 pixels, ombre et surbrillance ;
  - plan A prioritaire partout (tiles vides comprises), donc rien n'est
    assombri ;
  - le héros se dirige à la croix et saute avec A, B ou C ; ses
    collisions se font contre les voxels ;
  - son ombre est un sprite de couleur 15 de la ligne 3, posé sur le sol
    sous lui ; elle reste à la hauteur du sol pendant les sauts ;
  - sa priorité vient de `prio.bin`, et la caméra le suit.
- **Le banc sans affichage** (`sim/`) : Musashi (le 68000, licence MIT,
  laissé tel quel) et un modèle du VDP en C (plans, sprites, mode ombre et
  surbrillance). `cobrasim` y fait tourner la ROM et compte les cycles de
  chaque image ; `make sim` rejoue `demo/promenade.txt`.
- **Tests** sous AddressSanitizer et UBSan (gcc et clang) :
  - `test_scene` vérifie l'image contre un lancer de rayons, pixel par
    pixel (sur la démo et 100 zones au hasard). Il vérifie aussi le
    prédicat « voxel devant le héros » contre le rayon, le plan A et les
    priorités contre leur définition, et que les tiles redonnent l'image ;
  - si un assembleur est là, la ROM est réassemblée et doit être identique
    à celle du dépôt ;
  - `test_rom` vérifie le démarrage. Il vérifie aussi que le décor affiché
    est l'image attendue au pixel près, tout au long d'une promenade, que
    l'ombre assombrit de moitié, exactement, et que le héros est caché
    juste par ce qui est devant lui, à 1 297 positions.
- **Le point du rayon** : un pixel est vu au point (x + 1/2, y + 1). C'est
  la convention des triangles du treillis (rangée r : 2 (r + 1) pixels).
  Avec le centre du pixel, 4 176 pixels d'arête différaient du peintre ;
  avec ce point, aucun.

**Mesures** (zone de démo, `build/data/mesures.txt` et `make test`) :
- **Tiles** : 227 pour toute la zone, avec les retournements (229 sans).
  Le budget du décor est de 1 382 (1 472 - 80 pour le rayon - 10 pour les
  sprites), soit une marge de 1 155.
- **Plan A** : 23 738 pixels sur 101 888 pixels de décor (23,3 %), soit
  443 entrées non vides, contre 1 344 dans le plan B.
- **Palettes** : 34 tiles mêlaient les deux lignes ; 542 pixels ont pris
  la couleur la plus proche de l'autre ligne.
- **Positions du héros** : 7 704, dont 3 864 posées. Parmi ces dernières :
  - priorité basse : 58,9 % ;
  - priorité haute : 32,9 % ;
  - conflit : 8,2 % (318 positions), presque toutes dans la colonnade.
- **Conflits** : choisir la priorité la moins fausse fait passer les pixels
  faux de 7 509 à 3 245 sur les positions essayées ; 95 positions sur 115
  restent visiblement fausses.
- **Cycles par image** (V_INT et boucle principale) : 3 566 à 9 484, soit
  2,8 à 7,4 % d'une image NTSC (127 856 cycles).
- **Démarrage** : 10 images, parce que la VRAM est remplie par le
  processeur (pas encore de DMA).
- **BlastEm 0.6.3**, sans bordure et à l'échelle 2 : il montre les mêmes
  images que le banc, au pixel près (0 pixel hors correspondance). Cela
  vaut pour 4 scènes : le départ, le héros derrière une colonne, devant
  une colonne, et en conflit. Le mode ombre et surbrillance et la
  priorité des tiles transparentes se comportent comme dans le modèle.

Message de commit proposé :

```
Étape 0 : la zone affichée par le VDP, le héros et sa vraie ombre

Nouveau dépôt Cobra : un jeu en 3D isométrique façon Landstalker sur
Mega Drive seule. De DDS 32, on reprend le monde de voxels (MagicaVoxel,
cubes.txt) et le treillis de triangles ; fiche : ARCHITECTURE.md.

cobraconv range chaque pixel de décor dans le plan A s'il peut cacher
le héros à l'une de ses positions, sinon dans le plan B ; il produit les
tiles (227 pour la zone de démo, budget 1 382), deux lignes de palette
de décor et la priorité du héros à chaque position.

La ROM (asm68k, rom/cobra.bin déjà assemblée) : mode ombre et
surbrillance, plan A prioritaire partout, le héros à la croix avec saut,
son ombre par l'opérateur de la ligne 3, la caméra qui le suit ; 2,8 à
7,4 % d'une image NTSC.

Banc sans affichage : Musashi et un modèle du VDP, confronté à BlastEm
(mêmes images, au pixel près). Tests sous ASan et UBSan : image contre
un lancer de rayons, décor de la ROM contre l'image attendue,
recouvrements du héros à 1 297 positions (8,2 % en conflit).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01R9arX5VKsb6urMjsEiEwmU
```
