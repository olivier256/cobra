## Le rayon delta (Psychogun)

Décisions du 25 novembre 2026, issues de démos jouables. Cette section
remplace la décision précédente (rayon dessiné dans des copies de tiles du
décor, quatre couleurs réservées dans chaque ligne de palette du décor).

### Le principe de jeu

- **Voir, c'est verrouiller.** Tout pirate qui passe dans le quart de cercle
  de vision de Cobra (90°, soit 45° de chaque côté de sa direction) est
  verrouillé, quelle que soit la distance. Le verrouillage est instantané et
  définitif, où que le pirate aille ensuite. La vision est limitée à la pièce
  courante.
- **Marquage au sol.** Quatre triangles jaunes sous chaque pirate verrouillé,
  quatre triangles rouges sous la cible courante.
- **Boutons.** A : saut. B : tir. C : cible suivante.
- **Se concentrer, c'est tirer.** On maintient B : le jeu ralentit (20 % de la
  vitesse normale dans la démo). Pendant la concentration, la croix oriente P1
  dans l'une des 8 directions, et la durée de l'appui l'éloigne de Cobra.
  Au bout de 2 secondes, le tir part de lui-même ; sinon il part quand on
  relâche B.
- **Pas d'aperçu.** Le joueur ne voit ni la trajectoire ni P1. Un indicateur
  de concentration reste à concevoir : une jauge dessinée sur le Psychogun,
  vue en coupe, est la piste retenue.
- **Sans cible**, le tir part en ligne droite.
- **Puissance.** Avec moins d'une seconde de charge, le rayon s'arrête dans
  le premier pirate touché. Avec une seconde ou plus, il traverse tout
  jusqu'au mur.
- **Pirates en groupes** de deux ou trois, en ligne ou en triangle. En
  abattre plusieurs d'un seul tir donne un bonus.
- **Abandonné :**
  - placer P2 librement : le joueur avait l'impression de tracer des courbes
    de Bézier, comme dans un jeu éducatif ;
  - le verrouillage multiple : il fait perdre la concentration et donne des
    courbes en S ;
  - diriger le rayon en vol : à garder éventuellement pour un mode difficile.

### La géométrie

- **Une Bézier quadratique.** P0 est la main de Cobra, P1 = P0 + direction ×
  d, et P2 est la cible. Après P2, le rayon continue en ligne droite, dans la
  direction (P2 − P1), jusqu'à un mur. L'écart maximal du rayon par rapport à
  la ligne droite vaut d / 2.
- **Calcul par différences finies**, en virgule fixe 16.16, avec N étapes où
  N est une puissance de 2. Cela fait deux additions par axe et par point,
  sans aucune multiplication ; même l'initialisation se fait par décalages.
  On compte environ 3 000 cycles pour 32 points.
- **Calcul dans le monde 3D.** La projection isométrique est affine, donc la
  courbe projetée est la Bézier des points projetés.
- **Collisions.** Chaque point est comparé à la carte des hauteurs : deux
  décalages, une lecture de table et une comparaison.
- **Direction gratuite.** Le pas courant donne directement la tangente au
  rayon, qui sert à choisir l'angle des morceaux.

### Le rendu

- **Un segment de 160 pixels** qui parcourt la courbe, comme une comète.
  Dans le pire cas (segment horizontal), il occupe 168 pixels de sprites sur
  une ligne. Il en reste au moins 152 pour Cobra, son ombre et quatre
  pirates (96 pixels en tout). Un rayon de toute la longueur saturait les
  lignes, au-delà de 320 pixels.
- **Des sprites, faits de morceaux pré-dessinés.** On prépare 16 angles en
  VRAM : des morceaux de 32 × 8 pixels près de l'horizontale, de 8 × 32 près
  de la verticale, et de 16 × 16 entre les deux. Cela représente 64 à 100
  tiles, soit 2 à 3 Ko. Il n'y a aucun tracé de pixels pendant le jeu.
  Jointures dans les virages serrés : à vérifier.
- **Ligne de palette 3** pour Cobra et le rayon :

  | Entrées | Rôle |
  |---|---|
  | 1 à 10 | Cobra (rouge et noir) |
  | 11 | jaune |
  | 12 | jaune clair ou orangé |
  | 13 | blanc |
  | 14 | aura (opérateur de surbrillance) |
  | 15 | ombre de Cobra (opérateur d'ombre) |

  Les deux lignes du décor gardent leurs 15 couleurs.
- **Ordre dans la liste des sprites :** Cobra et son ombre, puis les sprites
  masques, puis le rayon, puis les pirates.
- **L'aura tranche.** Les opérateurs sont appliqués après la combinaison des
  sprites. L'aura ne peut donc pas éclaircir un pirate placé après le rayon
  dans la liste : elle le troue, et on voit à travers le décor éclairci. On
  s'en sert comme d'un effet : le pirate touché est tranché en deux. La zone
  de collision doit être aussi large que l'aura, pour que tout pirate troué
  soit bien un pirate abattu.

### Règles du mode ombre et surbrillance

Sources : Sega Technical Overview 1.00, pages 65 à 69 ; notes de Kabuto, section
« S/H ».

- La palette 3, couleur 15, est un opérateur d'ombre ; la palette 3,
  couleur 14, un opérateur de surbrillance. Ces pixels ne s'affichent pas.
- Un opérateur n'agit que sur les plans de priorité inférieure à celle du
  sprite.
- La surbrillance éclaircit un pixel normal et ramène un pixel assombri à la
  normale.
- Si les plans A et B sont tous deux en priorité basse, le décor est assombri.
- **Contrainte importante :** un sprite en priorité basse posé sur A et B en
  priorité basse est lui aussi assombri. Seule exception : les pixels de
  couleur 14 des palettes 0 à 2, qui ne le sont jamais. Cette règle est la
  pièce centrale du document à venir sur les plans et les priorités.

### Questions ouvertes

- L'indicateur de concentration : la jauge sur le Psychogun.
- L'ordre de défilement des cibles avec C : ordre de verrouillage, ou de
  gauche à droite à l'écran.
- Les réglages fins : durée maximale de charge, seuil du tir perçant (1 s),
  facteur de ralenti, vitesse du segment.
- La croix calée sur l'écran ou sur la manette : à valider manette en main,
  peut-être sous forme d'option.
- Une option hybride : un éclat d'impact en sprites.
- Le document sur les plans et les priorités, avec des schémas.
- À vérifier sur un émulateur précis : les jointures des morceaux, et le rendu
  de l'aura selon la priorité du décor.
