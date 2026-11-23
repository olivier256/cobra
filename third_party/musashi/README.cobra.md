# Musashi dans Cobra

Musashi, l'émulateur de 68000 de Karl Stenerud (licence MIT, voir
readme.txt), copié tel quel depuis https://github.com/kstenerud/Musashi
(commit 313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd), sans les dossiers
`test` et `example`.

Aucun fichier n'est modifié : la configuration du banc (68000 seul, appel
de `md_instruction_hook` avant chaque instruction) vient de
`sim/musashi_cobra.h`, inclus avant tout par le compilateur. `m68kops.c`
et `m68kops.h` sont produits à la construction par `m68kmake`.
