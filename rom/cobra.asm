; =============================================================================
; Cobra : étape 0. La zone de démo affichée par le VDP, le héros et son ombre.
;
; Assemblage (asm68k ou clownassembler_asm68k), depuis la racine du dépôt,
; après la conversion (make data) :
;     asm68k /p rom/cobra.asm,rom/cobra.bin
;
; - Le décor : deux plans de 64 x 64 tiles. Plan B : le fond, sans
;   priorité ; plan A : ce qui peut cacher le héros, prioritaire partout
;   (tiles vides comprises). Mode ombre et surbrillance : rien n'est
;   assombri, sauf sous l'ombre du héros.
; - Le héros : un sprite de 2 x 4 tiles (ligne 2), dirigé à la croix, saut
;   avec A, B ou C. Sa priorité vient de la table du convertisseur
;   (posinfo.bin, à chaque unité) : basse si le plan A qui le recouvre est
;   devant lui, haute s'il est derrière.
; - Son ombre : un sprite de 2 x 1 tiles, couleur 15 de la ligne 3
;   (l'opérateur d'ombre), posée sur le sol sous lui.
; - Les masques : quand le plan A qui recouvre le héros (ou son ombre) est
;   en partie devant et en partie derrière, les sprites passent en priorité
;   haute, et des sprites de 8 x 8 (masks.bin), placés avant eux dans la
;   liste, recopient par-dessus les pixels du décor qui sont devant.
; - La caméra suit le héros ; tout est écrit dans le VDP pendant la V_INT.
;
; Repère du monde (voir tools/conv.h) : u = 16 i, v = 16 j, w = 16 k ;
; dans l'image de la zone, x = u - v + ORIGIN_X + 16,
; y = (u + v) / 2 - w + ORIGIN_Y + 16.
; =============================================================================

	include	"build/data/zone.inc"

; ---- Matériel ---------------------------------------------------------------
VDP_DATA	EQU	$C00000
VDP_CTRL	EQU	$C00004
VERSION		EQU	$A10001
PAD1_DATA	EQU	$A10003
PAD1_CTRL	EQU	$A10009
Z80_BUSREQ	EQU	$A11100
Z80_RESET	EQU	$A11200
TMSS		EQU	$A14000

; ---- VRAM -------------------------------------------------------------------
VRAM_PLANE_A	EQU	$C000		; 64 x 64 x 2 = 8 Ko
VRAM_PLANE_B	EQU	$E000
VRAM_SPRITES	EQU	$B800		; table des sprites (80 x 8 octets)
VRAM_HSCROLL	EQU	$BC00
TILE_HERO	EQU	1462		; 8 tiles, puis l'ombre (2 tiles)
TILE_SHADOW	EQU	TILE_HERO+8

; Commande d'écriture du VDP pour l'adresse a (VRAM, CRAM, VSRAM)
CRAM_W		EQU	$C0000000
VSRAM_W		EQU	$40000010

vramw	MACRO	addr
	MOVE.L	#$40000000|(((\addr)&$3FFF)<<16)|((\addr)>>14),VDP_CTRL
	ENDM

; ---- Le héros ---------------------------------------------------------------
HERO_HALF	EQU	4		; demi-largeur de la boîte (unités)
HERO_HEIGHT	EQU	26		; hauteur de la boîte
JUMP_SPEED	EQU	80		; en 1/16 d'unité par image
GRAVITY		EQU	8
HERO_U0		EQU	40		; position de départ
HERO_V0		EQU	56
HERO_W0		EQU	32

; ---- RAM --------------------------------------------------------------------
	RSSET	$FFFF0000
heroU		RS.W	1		; position (unités)
heroV		RS.W	1
heroW		RS.W	1
heroWq		RS.W	1		; w en 1/16 d'unité
heroVw		RS.W	1		; vitesse verticale, 1/16 d'unité par image
heroGround	RS.W	1		; 1 si posé
shadowW		RS.W	1		; hauteur du sol sous le héros, -1 : aucun
padNow		RS.W	1		; boutons enfoncés (1 = enfoncé) : SACBRLDU
padPrev		RS.W	1
camX		RS.W	1
camY		RS.W	1
heroInfo	RS.L	1		; réglages (posent.bin) du héros et de l'ombre
shadowInfo	RS.L	1
sprCount	RS.W	1		; entrées dans sprBuf
sprBuf		RS.B	8*(2*MAX_MASKS+2)	; la table des sprites
vblank		RS.B	1		; mis à 1 par la V_INT
		RS.B	1
frameCount	RS.L	1
RAM_END		RS.B	0

; =============================================================================
	ORG	0
	DC.L	$00FFFE00		; pile
	DC.L	START
	DCB.L	22,INT_ERR
	DC.L	INT_ERR			; 24 : interruption parasite
	DC.L	INT_ERR, INT_ERR, INT_ERR
	DC.L	H_INT			; 28 : niveau 4
	DC.L	INT_ERR
	DC.L	V_INT			; 30 : niveau 6
	DC.L	INT_ERR
	DCB.L	32,INT_ERR		; TRAP, réservés

	DC.B	'SEGA MEGA DRIVE '
	DC.B	'(C)COBR 2026.NOV'
	DC.B	'COBRA - ETAPE 0                                 '
	DC.B	'COBRA - ETAPE 0                                 '
	DC.B	'GM 00000000-00'
	DC.W	0			; somme de contrôle (inutile ici)
	DC.B	'J               '
	DC.L	0, ROM_END-1
	DC.L	$FF0000, $FFFFFF
	DC.B	'            '
	DC.B	'            '
	DC.B	'                                        '
	DC.B	'JUE             '

; ---- Pour le banc de mesure (sim/) : à l'adresse $200 ----------------------
	DC.B	'COBR'
	DC.L	MAIN			; atteint quand le travail d'une image est fini
	DC.L	heroU			; l'état du héros en RAM
	DC.L	frameCount

; =============================================================================
START:
	MOVE.W	#$2700,SR
	MOVE.B	(VERSION).L,D0
	ANDI.B	#$0F,D0
	BEQ.S	@noTmss
	MOVE.L	#'SEGA',(TMSS).L
@noTmss
	MOVE.W	#$100,(Z80_BUSREQ).L	; le Z80 reste arrêté
	MOVE.W	#0,(Z80_RESET).L

	LEA	($FFFF0000).L,A0	; RAM à 0
	MOVE.W	#$4000-1,D0
@clr	CLR.L	(A0)+
	DBF	D0,@clr

	MOVE.W	(VDP_CTRL).L,D0		; état du VDP lu : commande en attente annulée
	LEA	VDP_REGS(PC),A0
	MOVEQ	#(VDP_REGS_END-VDP_REGS)/2-1,D0
@regs	MOVE.W	(A0)+,(VDP_CTRL).L
	DBF	D0,@regs

	LEA	(VDP_DATA).L,A1
	vramw	0			; VRAM à 0
	MOVE.W	#$4000-1,D0
@vclr	MOVE.L	#0,(A1)
	DBF	D0,@vclr

	MOVE.L	#CRAM_W,(VDP_CTRL).L	; palettes
	LEA	DATA_CRAM(PC),A0
	MOVEQ	#64-1,D0
@cram	MOVE.W	(A0)+,(A1)
	DBF	D0,@cram

	MOVE.L	#VSRAM_W,(VDP_CTRL).L
	MOVE.L	#0,(A1)

	vramw	0			; tiles du décor
	LEA	DATA_TILES,A0
	MOVE.W	#TILE_COUNT*8-1,D0
@tiles	MOVE.L	(A0)+,(A1)
	DBF	D0,@tiles

	vramw	TILE_HERO*32		; héros et ombre
	LEA	DATA_SPRITES,A0
	MOVE.W	#10*8-1,D0
@spr	MOVE.L	(A0)+,(A1)
	DBF	D0,@spr

	vramw	VRAM_PLANE_A
	LEA	DATA_PLANE_A,A0
	MOVE.W	#64*64/2-1,D0
@pa	MOVE.L	(A0)+,(A1)
	DBF	D0,@pa

	vramw	VRAM_PLANE_B
	LEA	DATA_PLANE_B,A0
	MOVE.W	#64*64/2-1,D0
@pb	MOVE.L	(A0)+,(A1)
	DBF	D0,@pb

	MOVE.B	#$40,(PAD1_CTRL).L	; TH en sortie
	MOVE.B	#$40,(PAD1_DATA).L

	MOVE.W	#HERO_U0,(heroU).L
	MOVE.W	#HERO_V0,(heroV).L
	MOVE.W	#HERO_W0,(heroW).L
	MOVE.W	#HERO_W0*16,(heroWq).L
	BSR	UPDATE_VIEW

	MOVE.W	#$8174,(VDP_CTRL).L	; affichage, V_INT, DMA, mode 5
	MOVE.W	#$2500,SR		; V_INT (niveau 6) acceptée

; =============================================================================
MAIN:
	CLR.B	(vblank).L
WAIT_LOOP:
	TST.B	(vblank).L
	BEQ.S	WAIT_LOOP
	BSR	READ_PAD
	BSR	UPDATE_HERO
	BSR	UPDATE_VIEW
	BRA.S	MAIN

; =============================================================================
; V_INT : écrit dans le VDP ce que la boucle principale a préparé.
V_INT:
	MOVEM.L	D0/A0-A1,-(SP)
	LEA	(VDP_DATA).L,A1
	vramw	VRAM_SPRITES
	LEA	(sprBuf).L,A0
	MOVE.W	(sprCount).L,D0
	ADD.W	D0,D0			; deux longs par entrée
	SUBQ.W	#1,D0
	BMI.S	@nospr
@spr	MOVE.L	(A0)+,(A1)
	DBF	D0,@spr
@nospr
	vramw	VRAM_HSCROLL
	MOVE.W	(camX).L,D0
	NEG.W	D0
	MOVE.W	D0,(A1)			; plan A
	MOVE.W	D0,(A1)			; plan B
	MOVE.L	#VSRAM_W,(VDP_CTRL).L
	MOVE.W	(camY).L,D0
	MOVE.W	D0,(A1)
	MOVE.W	D0,(A1)
	ADDQ.L	#1,(frameCount).L
	MOVE.B	#1,(vblank).L
	MOVEM.L	(SP)+,D0/A0-A1
H_INT:
	RTE

INT_ERR:
	MOVE.W	#$2700,SR
@stop	BRA.S	@stop

; =============================================================================
; Manette 3 boutons -> padNow (1 = enfoncé) : bits S A C B R L D U
READ_PAD:
	LEA	(PAD1_DATA).L,A0
	MOVE.W	(padNow).L,(padPrev).L
	MOVE.B	#$40,(A0)		; TH = 1 : C B R L D U
	NOP
	NOP
	MOVE.B	(A0),D0
	ANDI.W	#$3F,D0
	MOVE.B	#$00,(A0)		; TH = 0 : S A 0 0 D U
	NOP
	NOP
	MOVE.B	(A0),D1
	ANDI.W	#$30,D1
	LSL.W	#2,D1
	OR.W	D1,D0
	NOT.B	D0
	ANDI.W	#$FF,D0
	MOVE.W	D0,(padNow).L
	MOVE.B	#$40,(A0)
	RTS

; =============================================================================
; FREE : la boîte du héros posée en (D0, D1, D2) = (u, v, w) est-elle
; libre ? Z = 1 si libre. Dehors de la zone : plein. Garde D0-D2.
FREE:
	MOVEM.L	D0-D7/A0,-(SP)
	MOVE.W	D0,D3
	SUBQ.W	#HERO_HALF,D3
	BMI	@full
	MOVE.W	D1,D4
	SUBQ.W	#HERO_HALF,D4
	BMI	@full
	TST.W	D2
	BMI	@full
	MOVE.W	D0,D5
	ADDQ.W	#HERO_HALF-1,D5
	CMPI.W	#ZONE_W*16,D5
	BGE	@full
	MOVE.W	D1,D6
	ADDQ.W	#HERO_HALF-1,D6
	CMPI.W	#ZONE_D*16,D6
	BGE	@full
	LSR.W	#4,D3			; i0
	LSR.W	#4,D4			; j0
	LSR.W	#4,D5			; i1
	LSR.W	#4,D6			; j1
	MOVE.W	D2,D7
	ADDI.W	#HERO_HEIGHT-1,D7
	LSR.W	#4,D7			; k1
	CMPI.W	#ZONE_H,D7
	BLT.S	@kok
	MOVEQ	#ZONE_H-1,D7		; au-dessus de la zone : vide
@kok	LSR.W	#4,D2			; k0
	CMP.W	D7,D2
	BGT.S	@free
	LEA	DATA_VOXELS,A0
@k	MOVE.W	D4,D1			; j
@j	MOVE.W	D2,D0			; (k * D + j) * W + i0
	MULU	#ZONE_D,D0
	ADD.W	D1,D0
	MULU	#ZONE_W,D0
	ADD.W	D3,D0
	TST.B	(A0,D0.W)
	BNE.S	@full
	CMP.W	D3,D5
	BEQ.S	@nexti
	TST.B	1(A0,D0.W)
	BNE.S	@full
@nexti	ADDQ.W	#1,D1
	CMP.W	D6,D1
	BLE.S	@j
	ADDQ.W	#1,D2
	CMP.W	D7,D2
	BLE.S	@k
@free	MOVEM.L	(SP)+,D0-D7/A0
	ORI.B	#4,CCR			; Z = 1
	RTS
@full	MOVEM.L	(SP)+,D0-D7/A0
	ANDI.B	#$FB,CCR		; Z = 0
	RTS

; =============================================================================
UPDATE_HERO:
	MOVE.W	(heroU).L,D0
	MOVE.W	(heroV).L,D1
	MOVE.W	(heroW).L,D2
	; La croix, alignée sur l'écran : droite (u + 1, v - 1), haut
	; (u - 1, v - 1) ; les diagonales suivent un axe du monde.
	MOVEQ	#0,D3			; du
	MOVEQ	#0,D4			; dv
	MOVE.W	(padNow).L,D5
	BTST	#0,D5			; haut
	BEQ.S	@nup
	SUBQ.W	#1,D3
	SUBQ.W	#1,D4
@nup	BTST	#1,D5			; bas
	BEQ.S	@ndown
	ADDQ.W	#1,D3
	ADDQ.W	#1,D4
@ndown	BTST	#2,D5			; gauche
	BEQ.S	@nleft
	SUBQ.W	#1,D3
	ADDQ.W	#1,D4
@nleft	BTST	#3,D5			; droite
	BEQ.S	@nright
	ADDQ.W	#1,D3
	SUBQ.W	#1,D4
@nright
	TST.W	D3			; un axe après l'autre : on glisse le long
	BEQ.S	@nou			; des murs
	ADD.W	D3,D0
	BSR	FREE
	BEQ.S	@nou
	SUB.W	D3,D0
@nou	TST.W	D4
	BEQ.S	@nov
	ADD.W	D4,D1
	BSR	FREE
	BEQ.S	@nov
	SUB.W	D4,D1
@nov	MOVE.W	D0,(heroU).L
	MOVE.W	D1,(heroV).L

	; Posé ? (quelque chose juste sous les pieds)
	CLR.W	(heroGround).L
	SUBQ.W	#1,D2
	BSR	FREE
	BEQ.S	@air			; (D2 n'est plus utile dans la chute)
	ADDQ.W	#1,D2
	TST.W	(heroVw).L
	BGT.S	@air
	MOVE.W	#1,(heroGround).L
	CLR.W	(heroVw).L
	MOVE.W	(padNow).L,D5		; saut : A, B ou C vient d'être enfoncé
	MOVE.W	(padPrev).L,D6
	NOT.W	D6
	AND.W	D6,D5
	ANDI.W	#$70,D5
	BEQ	@done
	MOVE.W	#JUMP_SPEED,(heroVw).L
@air	; Chute ou saut
	MOVE.W	(heroVw).L,D6
	SUBQ.W	#GRAVITY,D6
	CMPI.W	#-128,D6
	BGE.S	@vok
	MOVE.W	#-128,D6
@vok	MOVE.W	D6,(heroVw).L
	MOVE.W	(heroWq).L,D7
	ADD.W	D6,D7
	MOVE.W	D7,D2
	ASR.W	#4,D2
	BSR	FREE
	BEQ.S	@moved
	TST.W	D6			; montée : la tête touche, on s'arrête
	BPL.S	@bump
	ADDI.W	#16,D2			; descente : posé sur le voxel touché
	ANDI.W	#$FFF0,D2
	MOVE.W	D2,D7
	LSL.W	#4,D7
	CLR.W	(heroVw).L
	BRA.S	@moved
@bump	CLR.W	(heroVw).L
	MOVE.W	(heroWq).L,D7
	MOVE.W	D7,D2
	ASR.W	#4,D2
@moved	MOVE.W	D7,(heroWq).L
	MOVE.W	D2,(heroW).L
@done	RTS

; =============================================================================
; UPDATE_VIEW : l'ombre, la caméra, les deux entrées de la table des sprites.
UPDATE_VIEW:
	MOVE.W	(heroU).L,D0
	MOVE.W	(heroV).L,D1
	MOVE.W	(heroW).L,D2

	; Le sol sous le héros : le premier niveau libre posé sur un voxel,
	; en descendant depuis ses pieds
	MOVE.W	D2,D3
	ANDI.W	#$FFF0,D3
@sh	MOVE.W	D3,D2
	SUBQ.W	#1,D2
	BMI.S	@noshadow
	BSR	FREE
	BNE.S	@found
	SUBI.W	#16,D3
	BRA.S	@sh
@noshadow
	MOVE.W	#-1,D3
@found	MOVE.W	D3,(shadowW).L
	MOVE.W	(heroW).L,D2

	; Les pieds dans l'image : x = u - v + ORIGIN_X + 16,
	; y = (u + v) / 2 - w + ORIGIN_Y + 16
	MOVE.W	D0,D4
	SUB.W	D1,D4
	ADDI.W	#ORIGIN_X+16,D4		; D4 = x
	MOVE.W	D0,D5
	ADD.W	D1,D5
	ASR.W	#1,D5
	ADDI.W	#ORIGIN_Y+16,D5		; D5 = y du sol (w = 0)

	; Caméra : le héros au milieu, sans sortir de l'image
	MOVE.W	D4,D6
	SUBI.W	#160,D6
	BPL.S	@cx0
	MOVEQ	#0,D6
@cx0	CMPI.W	#IMG_W-320,D6
	BLE.S	@cx1
	MOVE.W	#IMG_W-320,D6
@cx1	MOVE.W	D6,(camX).L
	MOVE.W	D5,D7
	SUB.W	D2,D7
	SUBI.W	#112+12,D7
	BPL.S	@cy0
	MOVEQ	#0,D7
@cy0	CMPI.W	#IMG_H-224,D7
	BLE.S	@cy1
	MOVE.W	#IMG_H-224,D7
@cy1	MOVE.W	D7,(camY).L

	; Les réglages du héros et de l'ombre (table des positions)
	MOVE.W	(heroW).L,D2
	BSR	LOOKUP
	MOVE.L	D0,(heroInfo).L
	MOVEQ	#-1,D0
	MOVE.W	(shadowW).L,D2
	BMI.S	@noinfo
	BSR	LOOKUP
@noinfo	MOVE.L	D0,(shadowInfo).L

	; La liste des sprites : les masques d'abord (devant), puis le héros,
	; puis l'ombre
	LEA	(sprBuf).L,A0
	CLR.W	(sprCount).L
	MOVE.L	(heroInfo).L,D0
	BSR	EMIT_MASKS
	MOVE.L	(shadowInfo).L,D0
	CMP.L	(heroInfo).L,D0
	BEQ.S	@same
	BSR	EMIT_MASKS
@same
	; Le héros : sprite 2 x 4, pieds à la ligne 30
	MOVE.W	(heroW).L,D2
	MOVE.W	D5,D3
	SUB.W	D2,D3
	SUB.W	D7,D3
	ADDI.W	#128-30,D3
	MOVE.W	D3,(A0)+		; y
	MOVE.W	#$0700,(A0)+		; 2 x 4 (le lien est mis plus bas)
	MOVE.W	#$4000|TILE_HERO,D3	; ligne 2
	MOVE.L	(heroInfo).L,D0
	BSR	HIGH_BIT
	OR.W	D0,D3
	MOVE.W	D3,(A0)+
	MOVE.W	D4,D3
	SUB.W	D6,D3
	ADDI.W	#128-8,D3
	MOVE.W	D3,(A0)+		; x
	ADDQ.W	#1,(sprCount).L

	; L'ombre : sprite 2 x 1, centré sur le sol sous les pieds
	MOVE.W	(shadowW).L,D2
	BMI.S	@links
	MOVE.W	D5,D3
	SUB.W	D2,D3
	SUB.W	D7,D3
	ADDI.W	#128-4,D3
	MOVE.W	D3,(A0)+
	MOVE.W	#$0400,(A0)+		; 2 x 1
	MOVE.W	#$6000|TILE_SHADOW,D3	; ligne 3
	MOVE.L	(shadowInfo).L,D0
	BSR	HIGH_BIT
	OR.W	D0,D3
	MOVE.W	D3,(A0)+
	MOVE.W	D4,D3
	SUB.W	D6,D3
	ADDI.W	#128-8,D3
	MOVE.W	D3,(A0)+
	ADDQ.W	#1,(sprCount).L

@links	; Les liens : chaque entrée vers la suivante, la dernière vers 0
	LEA	(sprBuf+3).L,A0
	MOVE.W	(sprCount).L,D0
	MOVEQ	#1,D1
@link	CMP.W	D0,D1
	BLT.S	@next
	CLR.B	(A0)			; la dernière
	RTS
@next	MOVE.B	D1,(A0)
	ADDQ.L	#8,A0
	ADDQ.W	#1,D1
	BRA.S	@link

; HIGH_BIT : D0.W = $8000 si le réglage D0.L demande la priorité haute
HIGH_BIT:
	CMPI.L	#-1,D0
	BEQ.S	@low
	SWAP	D0
	ANDI.W	#$8000,D0
	RTS
@low	MOVEQ	#0,D0
	RTS

; EMIT_MASKS : ajoute à la liste (A0, sprCount) les masques du réglage
; D0.L. Garde D4-D7.
EMIT_MASKS:
	CMPI.L	#-1,D0
	BEQ.S	@out
	ANDI.L	#$00FFFFFF,D0
	LEA	DATA_MASKS,A1
	ADDA.L	D0,A1
	MOVE.W	(A1)+,D1		; nombre de masques
	SUBQ.W	#1,D1
	BMI.S	@out
@mask	MOVEQ	#0,D0
	MOVE.B	1(A1),D0		; ty
	LSL.W	#3,D0
	SUB.W	D7,D0
	ADDI.W	#128,D0
	MOVE.W	D0,(A0)+		; y
	CLR.W	(A0)+			; 1 x 1 (le lien est mis plus tard)
	MOVE.W	2(A1),D0
	ORI.W	#$8000,D0		; prioritaire
	MOVE.W	D0,(A0)+
	MOVEQ	#0,D0
	MOVE.B	(A1),D0			; tx
	LSL.W	#3,D0
	SUB.W	D6,D0
	ADDI.W	#128,D0
	MOVE.W	D0,(A0)+		; x
	ADDQ.L	#4,A1
	ADDQ.W	#1,(sprCount).L
	DBF	D1,@mask
@out	RTS

; LOOKUP : D0.L = le réglage (posent.bin) de la position (heroU, heroV,
; D2) ; -1 hors de la table. La case de 4 x 4 unités dans posinfo.bin ;
; si elle est détaillée, la position dans possub.bin.
LOOKUP:
	MOVEM.L	D1-D3/A1,-(SP)
	MOVE.W	(heroU).L,D0
	LSR.W	#2,D0			; m
	MOVE.W	(heroV).L,D1
	LSR.W	#2,D1			; n
	MOVE.W	D2,D3
	ADDQ.W	#8,D3
	LSR.W	#4,D3			; k, arrondi
	CMPI.W	#ZONE_H,D3
	BGE.S	@none
	MULU	#POS_MV,D3
	ADD.W	D1,D3
	MULU	#POS_MU,D3
	ADD.W	D0,D3
	ADD.L	D3,D3
	LEA	DATA_POSINFO,A1
	MOVE.W	(A1,D3.L),D0		; la case
	BPL.S	@entry
	ANDI.L	#$7FFF,D0		; détaillée : d x 16 + (v & 3) x 4 + (u & 3)
	LSL.L	#4,D0
	MOVE.W	(heroV).L,D1
	ANDI.W	#3,D1
	LSL.W	#2,D1
	ADD.W	D1,D0
	MOVE.W	(heroU).L,D1
	ANDI.W	#3,D1
	ADD.W	D1,D0
	ADD.L	D0,D0
	LEA	DATA_POSSUB,A1
	MOVE.W	(A1,D0.L),D0
@entry	ANDI.L	#$7FFF,D0		; le numéro du réglage
	LSL.L	#2,D0
	LEA	DATA_POSENT,A1
	MOVE.L	(A1,D0.L),D0
	BRA.S	@out
@none	MOVEQ	#-1,D0
@out	MOVEM.L	(SP)+,D1-D3/A1
	RTS

; =============================================================================
VDP_REGS:
	DC.W	$8004		; 0 : H_INT coupée
	DC.W	$8134		; 1 : affichage coupé pendant le chargement, V_INT, DMA
	DC.W	$8200|(VRAM_PLANE_A>>10)
	DC.W	$8300		; 3 : fenêtre (inutilisée)
	DC.W	$8400|(VRAM_PLANE_B>>13)
	DC.W	$8500|(VRAM_SPRITES>>9)
	DC.W	$8600
	DC.W	$8700		; 7 : fond = ligne 0, couleur 0
	DC.W	$8800
	DC.W	$8900
	DC.W	$8AFF		; 10 : compteur H_INT
	DC.W	$8B00		; 11 : défilement plein écran
	DC.W	$8C89		; 12 : 320 pixels, ombre et surbrillance
	DC.W	$8D00|(VRAM_HSCROLL>>10)
	DC.W	$8E00
	DC.W	$8F02		; 15 : auto-incrément 2
	DC.W	$9011		; 16 : plans de 64 x 64
	DC.W	$9100		; 17 : pas de fenêtre
	DC.W	$9200
VDP_REGS_END:
	EVEN

DATA_CRAM:
	INCBIN	"build/data/cram.bin"
DATA_SPRITES:
	INCBIN	"build/data/sprites.bin"
DATA_TILES:
	INCBIN	"build/data/tiles.bin"
DATA_PLANE_A:
	INCBIN	"build/data/plane_a.bin"
DATA_PLANE_B:
	INCBIN	"build/data/plane_b.bin"
DATA_VOXELS:
	INCBIN	"build/data/voxels.bin"
	EVEN
DATA_MASKS:
	INCBIN	"build/data/masks.bin"
	EVEN
DATA_POSENT:
	INCBIN	"build/data/posent.bin"
DATA_POSINFO:
	INCBIN	"build/data/posinfo.bin"
DATA_POSSUB:
	INCBIN	"build/data/possub.bin"
	IF	*<=$20000		; 128 Ko, sinon 256 Ko
	DCB.B	$20000-*,$FF
	ELSE
	DCB.B	$40000-*,$FF
	ENDIF
ROM_END:
