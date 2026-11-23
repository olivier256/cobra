# Cobra : fiche d'architecture

« Cobra » est un nom de projet interne. Le but : un jeu original en 3D
isométrique dans l'esprit de Landstalker, sur Mega Drive seule (console ou
émulateur), dans un monde quasi ouvert, avec ponts et arches, au moins deux
lignes de palette pour le décor et le moins de tramage possible. Les
personnages et les graphismes définitifs seront fournis à part ; ceux du
dépôt sont provisoires.

Cette fiche résume les choix arrêtés ensemble et ce que l'étape 0 en a
vérifié.

## Le principe : celui du DDS520

Comme Landstalker (le DDS520 et « Mirage Type III », sur PC-98), Cobra ne
dessine jamais un cube sur la console : **le travail 3D se fait sur Mac**,
par le convertisseur `cobraconv`, qui aplatit le monde en tiles et en tables
de plans ; **le VDP affiche** ces tiles par le matériel ; **le 68000 fait le
jeu**. Leçon de DDS 32 (32X) : redessiner l'écran coûtait 78 à 88 % du
budget ; ici, le décor ne coûte rien au processeur.

Repris de DDS 32 : le monde de voxels édité dans MagicaVoxel, `cubes.txt`,
les images de cube 32 x 32, le treillis de triangles (`iso/iso.h`), la
lecture des BMP. Le reste est neuf.

## Le monde

- Une zone : W x D cases, H niveaux (H <= 16), chaque emplacement vide ou
  plein d'un type de cube (1 à 255) ; repère du DDS520 : une case fait un
  losange de 32 x 16 pixels, un niveau 16 pixels, 16 unités par case.
- Dans l'image de la zone, le point du monde (u, v, w) tombe en
  x = u - v + originX + 16, y = (u + v) / 2 - w + originY + 16.
- Zone de démo : 17 x 15 x 8 (W + D = 32), soit une image de 512 x 384
  pixels, qui tient dans un plan de 64 x 64 tiles : la caméra défile sans
  rien recharger. Les zones plus grandes demanderont de recharger les
  tables des plans en défilant (étape suivante).

## Palettes (mode 320 pixels)

| Ligne | Usage | Couleurs |
|-------|-------|----------|
| 0 | décor gris | 0 fond, 1 à 11 décor, 12 à 15 rayon delta |
| 1 | décor bleu | 0 (inutilisé), 1 à 11 décor, 12 à 15 rayon delta |
| 2 | le héros | 1 à 11 héros (rouge, bottes noires), 12 à 15 rayon delta |
| 3 | les ennemis | 1 à 13 ; 14 et 15 : opérateurs de surbrillance et d'ombre |

Le rayon delta est un dégradé du jaune au blanc (quatre teintes, sans
tramage), présent dans les trois lignes : il sera dessiné dans des copies
des tiles du décor (réserve de 80 tiles en VRAM) et sur le héros. Si 11
couleurs manquent au décor, on arbitrera.

Dans les images de cube, l'index de la palette commune dit la ligne :
16 à 26 → ligne 0, 32 à 42 → ligne 1, 48 à 63 → ligne 2 (voir
`tools/cubes2bmp.py`). Les couleurs sont celles de la Mega Drive (niveaux
0 à 7, x 36). **Un cube ne mélange pas les lignes**, sinon les tiles de ses
arêtes en mêleraient deux. Un tile du décor où deux cubes de lignes
différentes se touchent prend la ligne qui change le moins l'image (les
autres pixels prennent la couleur la plus proche) : c'est mesuré.

Le sol ne doit pas être noir : assombri de moitié, le noir reste noir et
l'ombre ne se voit plus.

## Les deux plans, l'ombre et la priorité

Mode ombre et surbrillance (registre 12) : un pixel des plans est assombri
si ni le tile du plan A ni celui du plan B n'a le bit de priorité ; la
priorité d'un tile compte même là où il est transparent (vérifié dans
BlastEm). D'où :

- **plan B** : le fond, sans priorité ;
- **plan A** : ce qui peut cacher le héros, **prioritaire partout**, tiles
  vides comprises : rien n'est assombri ;
- **l'ombre du héros** : un sprite de couleur 15 de la ligne 3, posé sur le
  sol sous lui : il assombrit ce qu'il recouvre, sans prendre une couleur
  aux palettes. Elle dit où le héros retombera (le défaut de Landstalker).

Le convertisseur range un pixel de décor dans le plan A **si et seulement
si** il peut cacher le héros à l'une des positions où le héros peut se
trouver : le pixel est dans le rectangle de son sprite (16 x 32), et son
voxel est devant la boîte du héros (boîtes séparées le long d'un axe vers
l'œil ; vérifié contre un lancer de rayons). Positions : tous les 4 unités,
à chaque niveau, posées sur un voxel ou un niveau au-dessus (saut).

Pour chaque position, `prio.bin` donne la priorité du sprite : basse si les
pixels du plan A qui le recouvrent sont tous devant lui, haute s'ils sont
tous derrière. S'il y en a des deux sortes, c'est un **conflit** (le héros
entre deux colonnes) : un seul bit ne peut pas faire juste ; on prend la
priorité qui fait le moins de pixels faux, et on le compte. Retirer la
priorité des tiles derrière lui (l'astuce de Landstalker) les assombrirait
en mode ombre et surbrillance : exclu. Pistes pour plus tard : découper le
sprite, redessiner par sprites les quelques pixels du premier plan, ou
déplacer dans le plan B un objet devant lequel le héros passe.

## La VRAM

| Adresse | Contenu |
|---------|---------|
| $0000-$B7FF | 1 472 tiles : décor, réserve du rayon delta (80), héros et ombre (10) |
| $B800 | table des sprites |
| $BC00 | défilement horizontal |
| $C000 | table du plan A (64 x 64) |
| $E000 | table du plan B (64 x 64) |

Budget du décor : 1 472 - 80 - 10 = 1 382 tiles.

## La ROM

`rom/cobra.asm`, en asm68k (assemblé par asm68k ou par clownassembler,
son clone libre). Au démarrage, tout est copié dans le VDP par le
processeur (pas encore de DMA). À chaque image : la V_INT écrit la table
des sprites et le défilement ; la boucle principale lit la manette, déplace
le héros (collisions contre les voxels, saut), cherche le sol sous lui pour
l'ombre, lit sa priorité dans `prio.bin` et place la caméra.

## Le banc sans affichage

`sim/` : Musashi (le 68000) et un modèle du VDP en C (ports, plans, sprites,
mode ombre et surbrillance). Il exécute la ROM image par image, au cycle
près pour le 68000 (pas les attentes du VDP ni le DMA), et compte les
cycles de chaque image. `make test` y fait tourner la ROM et compare ce
qu'elle affiche à ce que le convertisseur attend. Le modèle a été confronté
à BlastEm : mêmes images, au pixel près.
