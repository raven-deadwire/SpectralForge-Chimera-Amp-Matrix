_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.weak_def_can_be_hidden	__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.p2align	2
__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb: ; @_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #176
	stp	x28, x27, [sp, #80]             ; 16-byte Folded Spill
	stp	x26, x25, [sp, #96]             ; 16-byte Folded Spill
	stp	x24, x23, [sp, #112]            ; 16-byte Folded Spill
	stp	x22, x21, [sp, #128]            ; 16-byte Folded Spill
	stp	x20, x19, [sp, #144]            ; 16-byte Folded Spill
	stp	x29, x30, [sp, #160]            ; 16-byte Folded Spill
	add	x29, sp, #160
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	.cfi_offset w21, -40
	.cfi_offset w22, -48
	.cfi_offset w23, -56
	.cfi_offset w24, -64
	.cfi_offset w25, -72
	.cfi_offset w26, -80
	.cfi_offset w27, -88
	.cfi_offset w28, -96
	str	x1, [sp, #48]                   ; 8-byte Folded Spill
	mov	x20, x0
	tbz	w3, #0, LBB97_33
; %bb.1:
	ldp	x0, x8, [x20, #88]
	sub	x1, x8, x0
	cmp	x1, #1
	b.lt	LBB97_3
; %bb.2:
	mov	x21, x2
	bl	_bzero
	mov	x2, x21
LBB97_3:
	ldp	w3, w0, [x20, #28]
	cmp	w0, #1
	b.le	LBB97_34
; %bb.4:
	mov	x8, #0                          ; =0x0
	ldr	x9, [x20, #64]
	sxtw	x12, w2
	lsl	w13, w3, #1
	smull	x10, w12, w13
	stp	x10, x9, [sp, #32]              ; 16-byte Folded Spill
	add	x19, x9, x10, lsl #2
	smull	x12, w13, w12
	add	x15, x9, x12, lsl #2
	lsr	x17, x2, #32
	add	x9, x15, #16
	stp	x9, x15, [sp, #8]               ; 16-byte Folded Spill
	mov	w14, #1                         ; =0x1
	mov	w24, #4                         ; =0x4
	mov	w16, #4                         ; =0x4
                                        ; kill: def $w17 killed $w17 killed $x17 def $x17
	str	x19, [sp, #24]                  ; 8-byte Folded Spill
	b	LBB97_6
LBB97_5:                                ;   in Loop: Header=BB97_6 Depth=1
	add	x14, x14, #1
	ldrsw	x0, [x20, #32]
	add	x8, x8, #1
	add	x16, x16, #4
	cmp	x14, x0
	b.ge	LBB97_34
LBB97_6:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB97_11 Depth 2
                                        ;     Child Loop BB97_27 Depth 2
                                        ;     Child Loop BB97_31 Depth 2
	add	w13, w17, #1
	cmp	w13, w0
	csinc	w17, wzr, w17, ge
	lsl	w13, w3, #1
	sxtw	x2, w13
	smull	x7, w13, w17
	ldr	x13, [x20]
	ldr	x1, [x13, #32]
	ldr	x0, [x20, #88]
	and	w13, w3, #0xfffffffc
	cmp	w13, #1
	b.lt	LBB97_9
; %bb.7:                                ;   in Loop: Header=BB97_6 Depth=1
	add	x4, x19, x7, lsl #2
	cmp	w13, #4
	csel	w13, w13, w24, hi
	sub	x5, x13, #1
	cmp	x5, #3
	b.ne	LBB97_10
; %bb.8:                                ;   in Loop: Header=BB97_6 Depth=1
	mov	x3, #0                          ; =0x0
	b	LBB97_12
LBB97_9:                                ;   in Loop: Header=BB97_6 Depth=1
	mov	x22, #0                         ; =0x0
	cmp	w22, w3
	b.ge	LBB97_5
	b	LBB97_15
LBB97_10:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x6, #0                          ; =0x0
	mov	x3, #0                          ; =0x0
	lsl	x13, x5, #2
	and	x13, x13, #0xfffffffffffffff0
	add	x13, x13, #16
	and	x21, x13, #0xffffffffffffffe0
	madd	x22, x16, x2, x1
LBB97_11:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldrsw	x9, [x20, #28]
	add	x10, x4, x6
	ldr	q0, [x10]
	lsl	x9, x9, #2
	add	x11, x22, x6
	ldr	q1, [x11]
	add	x12, x0, x6
	ldr	q2, [x11, x9]
	ldr	q3, [x12]
	ldr	q4, [x12, x9]
	fmla.4s	v3, v1, v0
	ldr	q5, [x10, x9]
	fmls.4s	v3, v2, v5
	fmla.4s	v4, v2, v0
	fmla.4s	v4, v1, v5
	str	q3, [x12]
	ldrsw	x9, [x20, #28]
	lsl	x9, x9, #2
	str	q4, [x12, x9]
	ldrsw	x9, [x20, #28]
	lsl	x9, x9, #2
	add	x13, x10, x9
	ldr	q0, [x10, #16]
	ldr	q1, [x13, #16]
	add	x10, x11, x9
	ldr	q2, [x11, #16]
	add	x9, x12, x9
	ldr	q3, [x12, #16]
	ldr	q4, [x10, #16]
	ldr	q5, [x9, #16]
	fmla.4s	v3, v2, v0
	fmls.4s	v3, v4, v1
	fmla.4s	v5, v4, v0
	fmla.4s	v5, v2, v1
	str	q3, [x12, #16]
	ldrsw	x9, [x20, #28]
	add	x9, x12, x9, lsl #2
	str	q5, [x9, #16]
	add	x3, x3, #8
	add	x6, x6, #32
	cmp	x21, x6
	b.ne	LBB97_11
LBB97_12:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w5, #2, LBB97_14
; %bb.13:                               ;   in Loop: Header=BB97_6 Depth=1
	mul	x13, x14, x2
	add	x13, x1, x13, lsl #2
	lsl	x5, x3, #2
	add	x4, x4, x5
	ldr	q0, [x4]
	ldrsw	x6, [x20, #28]
	lsl	x6, x6, #2
	ldr	q1, [x4, x6]
	add	x13, x13, x5
	ldr	q2, [x13]
	ldr	q3, [x13, x6]
	add	x13, x0, x5
	ldr	q4, [x13]
	ldr	q5, [x13, x6]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x13]
	ldrsw	x4, [x20, #28]
	lsl	x4, x4, #2
	str	q5, [x13, x4]
	add	x3, x3, #4
LBB97_14:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	w22, w3
	ldr	w3, [x20, #28]
	cmp	w22, w3
	b.ge	LBB97_5
LBB97_15:                               ;   in Loop: Header=BB97_6 Depth=1
	sxtw	x4, w17
	sxtw	x5, w3
	mov	w6, w3
	sub	x9, x6, x22
	cmp	x9, #11
	b.hi	LBB97_17
; %bb.16:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x7, x22
	b	LBB97_30
LBB97_17:                               ;   in Loop: Header=BB97_6 Depth=1
	stp	x9, x4, [sp, #56]               ; 16-byte Folded Spill
	lsl	x13, x8, #2
	add	x13, x13, #4
	lsl	x23, x22, #2
	lsl	x24, x6, #2
	add	x25, x5, x22
	lsl	x25, x25, #2
	ldr	x9, [sp, #32]                   ; 8-byte Folded Reload
	add	x7, x9, x7
	add	x7, x7, x6
	mov	x11, x1
	ldr	x1, [sp, #40]                   ; 8-byte Folded Reload
	add	x26, x1, x7, lsl #2
	add	x10, x0, x25
	add	x15, x0, x24
	add	x27, x5, x6
	lsl	x27, x27, #2
	add	x9, x0, x27
	smull	x28, w4, w2
	add	x28, x19, x28, lsl #2
	add	x30, x28, x23
	str	x11, [sp, #72]                  ; 8-byte Folded Spill
	madd	x11, x13, x2, x11
	add	x12, x11, x23
	add	x28, x28, x25
	add	x13, x7, x5
	add	x19, x1, x13, lsl #2
	add	x4, x11, x25
	add	x21, x0, x23
	cmp	x21, x26
	ccmp	x30, x15, #2, lo
	cset	w13, lo
	add	x1, x11, x24
	cmp	x21, x1
	ccmp	x12, x15, #2, lo
	cset	w7, lo
	cmp	x21, x19
	ccmp	x28, x15, #2, lo
	cset	w24, lo
	add	x11, x11, x27
	cmp	x21, x11
	ccmp	x4, x15, #2, lo
	cset	w25, lo
	cmp	x10, x26
	ccmp	x30, x9, #2, lo
	cset	w26, lo
	cmp	x10, x1
	ccmp	x12, x9, #2, lo
	cset	w27, lo
	cmp	x10, x19
	ccmp	x28, x9, #2, lo
	cset	w28, lo
	cmp	x10, x11
	ccmp	x4, x9, #2, lo
	cset	w30, lo
	cmp	x10, x15
	ccmp	x21, x9, #2, lo
	b.lo	LBB97_32
; %bb.18:                               ;   in Loop: Header=BB97_6 Depth=1
	ldr	x15, [sp, #16]                  ; 8-byte Folded Reload
	ldp	x4, x1, [sp, #64]               ; 16-byte Folded Reload
	tbnz	w13, #0, LBB97_29
; %bb.19:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w7, #0, LBB97_29
; %bb.20:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w24, #0, LBB97_29
; %bb.21:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w25, #0, LBB97_29
; %bb.22:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w26, #0, LBB97_29
; %bb.23:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w27, #0, LBB97_29
; %bb.24:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w28, #0, LBB97_29
; %bb.25:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w30, #0, LBB97_29
; %bb.26:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x25, #0                         ; =0x0
	ldr	x19, [sp, #56]                  ; 8-byte Folded Reload
	and	x13, x19, #0xfffffffffffffff8
	add	x7, x13, x22
	smull	x9, w4, w2
	lsl	x9, x9, #2
	add	x10, x5, x22
	lsl	x10, x10, #2
	ldr	x11, [sp, #8]                   ; 8-byte Folded Reload
	add	x11, x11, x10
	add	x22, x11, x9
	add	x26, x0, x23
	mul	x11, x16, x2
	add	x12, x15, x23
	add	x27, x12, x9
	add	x9, x11, x10
	add	x9, x1, x9
	add	x28, x9, #16
	add	x9, x0, x10
	add	x30, x9, #16
	add	x9, x11, x23
	add	x23, x1, x9
	mov	x24, x13
LBB97_27:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	add	x9, x26, x25
	ldp	q0, q1, [x9]
	add	x10, x27, x25
	ldp	q2, q3, [x10]
	add	x10, x23, x25
	ldp	q4, q5, [x10]
	fmla.4s	v0, v4, v2
	fmla.4s	v1, v5, v3
	add	x10, x22, x25
	ldp	q6, q7, [x10, #-16]
	add	x10, x28, x25
	ldp	q16, q17, [x10, #-16]
	fmls.4s	v0, v16, v6
	fmls.4s	v1, v17, v7
	add	x10, x30, x25
	ldp	q18, q19, [x10, #-16]
	fmla.4s	v18, v16, v2
	fmla.4s	v19, v17, v3
	fmla.4s	v18, v4, v6
	fmla.4s	v19, v5, v7
	stp	q0, q1, [x9]
	stp	q18, q19, [x10, #-16]
	add	x25, x25, #32
	subs	x24, x24, #8
	b.ne	LBB97_27
; %bb.28:                               ;   in Loop: Header=BB97_6 Depth=1
	cmp	x19, x13
	ldr	x19, [sp, #24]                  ; 8-byte Folded Reload
	mov	w24, #4                         ; =0x4
	b.eq	LBB97_5
	b	LBB97_30
LBB97_29:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x7, x22
	ldr	x19, [sp, #24]                  ; 8-byte Folded Reload
	mov	w24, #4                         ; =0x4
LBB97_30:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x21, #0                         ; =0x0
	sub	x6, x6, x7
	add	x13, x7, x5
	lsl	x5, x13, #2
	add	x13, x0, x5
	mul	x22, x16, x2
	smull	x4, w4, w2
	add	x2, x22, x5
	add	x2, x1, x2
	lsl	x23, x4, #2
	add	x4, x15, x5
	add	x4, x4, x23
	lsl	x7, x7, #2
	add	x5, x22, x7
	add	x1, x1, x5
	add	x5, x15, x7
	add	x5, x5, x23
	add	x0, x0, x7
LBB97_31:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	s0, [x0, x21, lsl #2]
	ldr	s1, [x5, x21, lsl #2]
	ldr	s2, [x1, x21, lsl #2]
	ldr	s3, [x4, x21, lsl #2]
	ldr	s4, [x2, x21, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x13, x21, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x0, x21, lsl #2]
	str	s1, [x13, x21, lsl #2]
	add	x21, x21, #1
	cmp	x6, x21
	b.ne	LBB97_31
	b	LBB97_5
LBB97_32:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x7, x22
	ldp	x15, x19, [sp, #16]             ; 16-byte Folded Reload
	mov	w24, #4                         ; =0x4
	ldp	x4, x1, [sp, #64]               ; 16-byte Folded Reload
	b	LBB97_30
LBB97_33:
	ldr	w3, [x20, #28]
LBB97_34:
	ldr	x0, [x20, #112]
	ldr	x1, [x20, #88]
	lsl	w2, w3, #1
	bl	__ZN4juce25FloatVectorOperationsBaseIfiE4copyEPfPKfi
	ldr	x8, [x20]
	ldr	x8, [x8, #32]
	ldr	x9, [x20, #112]
	ldr	w12, [x20, #28]
	and	w10, w12, #0xfffffffc
	cmp	w10, #1
	b.lt	LBB97_37
; %bb.35:
	mov	w11, #4                         ; =0x4
	cmp	w10, #4
	csel	w10, w10, w11, hi
	sub	x11, x10, #1
	cmp	x11, #3
	ldr	x1, [sp, #48]                   ; 8-byte Folded Reload
	b.ne	LBB97_38
; %bb.36:
	mov	x10, #0                         ; =0x0
	b	LBB97_40
LBB97_37:
	mov	w10, #0                         ; =0x0
	ldr	x1, [sp, #48]                   ; 8-byte Folded Reload
	cmp	w10, w12
	b.lt	LBB97_43
	b	LBB97_47
LBB97_38:
	mov	x12, #0                         ; =0x0
	mov	x10, #0                         ; =0x0
	lsl	x13, x11, #2
	and	x13, x13, #0xfffffffffffffff0
	add	x13, x13, #16
	and	x13, x13, #0xffffffffffffffe0
LBB97_39:                               ; =>This Inner Loop Header: Depth=1
	ldrsw	x14, [x20, #28]
	add	x15, x1, x12
	ldr	q0, [x15]
	lsl	x14, x14, #2
	add	x16, x8, x12
	ldr	q1, [x16]
	add	x17, x9, x12
	ldr	q2, [x16, x14]
	ldr	q3, [x17]
	ldr	q4, [x17, x14]
	fmla.4s	v3, v1, v0
	ldr	q5, [x15, x14]
	fmls.4s	v3, v2, v5
	fmla.4s	v4, v2, v0
	fmla.4s	v4, v1, v5
	str	q3, [x17]
	ldrsw	x14, [x20, #28]
	lsl	x14, x14, #2
	str	q4, [x17, x14]
	ldrsw	x14, [x20, #28]
	lsl	x14, x14, #2
	add	x0, x15, x14
	ldr	q0, [x15, #16]
	ldr	q1, [x0, #16]
	add	x15, x16, x14
	ldr	q2, [x16, #16]
	add	x14, x17, x14
	ldr	q3, [x17, #16]
	ldr	q4, [x15, #16]
	ldr	q5, [x14, #16]
	fmla.4s	v3, v2, v0
	fmls.4s	v3, v4, v1
	fmla.4s	v5, v4, v0
	fmla.4s	v5, v2, v1
	str	q3, [x17, #16]
	ldrsw	x14, [x20, #28]
	add	x14, x17, x14, lsl #2
	str	q5, [x14, #16]
	add	x10, x10, #8
	add	x12, x12, #32
	cmp	x13, x12
	b.ne	LBB97_39
LBB97_40:
	tbnz	w11, #2, LBB97_42
; %bb.41:
	lsl	x11, x10, #2
	add	x12, x1, x11
	ldr	q0, [x12]
	ldrsw	x13, [x20, #28]
	lsl	x13, x13, #2
	ldr	q1, [x12, x13]
	add	x12, x8, x11
	ldr	q2, [x12]
	ldr	q3, [x12, x13]
	add	x11, x9, x11
	ldr	q4, [x11]
	ldr	q5, [x11, x13]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x11]
	ldrsw	x12, [x20, #28]
	lsl	x12, x12, #2
	str	q5, [x11, x12]
	add	x10, x10, #4
LBB97_42:
	ldr	w12, [x20, #28]
	cmp	w10, w12
	b.ge	LBB97_47
LBB97_43:
	mov	w15, w10
	sxtw	x11, w12
	mov	w10, w12
	sub	x12, x10, x15
	cmp	x12, #19
	b.hi	LBB97_48
LBB97_44:
	mov	x14, x15
LBB97_45:
	lsl	x13, x11, #2
	add	x11, x9, x13
	add	x12, x8, x13
	add	x13, x1, x13
LBB97_46:                               ; =>This Inner Loop Header: Depth=1
	ldr	s0, [x9, x14, lsl #2]
	ldr	s1, [x1, x14, lsl #2]
	ldr	s2, [x8, x14, lsl #2]
	ldr	s3, [x13, x14, lsl #2]
	ldr	s4, [x12, x14, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x11, x14, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x9, x14, lsl #2]
	str	s1, [x11, x14, lsl #2]
	add	x14, x14, #1
	cmp	x10, x14
	b.ne	LBB97_46
LBB97_47:
	ldp	x29, x30, [sp, #160]            ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #144]            ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #128]            ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #112]            ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #96]             ; 16-byte Folded Reload
	ldp	x28, x27, [sp, #80]             ; 16-byte Folded Reload
	add	sp, sp, #176
	ret
LBB97_48:
	lsl	x13, x15, #2
	add	x5, x9, x13
	lsl	x16, x10, #2
	add	x14, x11, x15
	lsl	x14, x14, #2
	mov	x22, x1
	add	x1, x1, x16
	cmp	x5, x1
	add	x6, x9, x14
	add	x7, x9, x16
	add	x17, x11, x10
	lsl	x0, x17, #2
	add	x19, x9, x0
	add	x2, x22, x13
	add	x3, x8, x13
	add	x4, x22, x14
	add	x20, x8, x14
	ccmp	x2, x7, #2, lo
	cset	w14, lo
	add	x21, x8, x16
	cmp	x5, x21
	ccmp	x3, x7, #2, lo
	cset	w16, lo
	add	x22, x22, x0
	cmp	x5, x22
	ccmp	x4, x7, #2, lo
	cset	w17, lo
	add	x23, x8, x0
	cmp	x5, x23
	ccmp	x20, x7, #2, lo
	cset	w0, lo
	cmp	x6, x1
	ccmp	x2, x19, #2, lo
	cset	w1, lo
	cmp	x6, x21
	ccmp	x3, x19, #2, lo
	cset	w2, lo
	cmp	x6, x22
	ccmp	x4, x19, #2, lo
	cset	w3, lo
	cmp	x6, x23
	ccmp	x20, x19, #2, lo
	cset	w4, lo
	cmp	x6, x7
	ccmp	x5, x19, #2, lo
	b.lo	LBB97_60
; %bb.49:
	tbnz	w14, #0, LBB97_60
; %bb.50:
	tbnz	w16, #0, LBB97_60
; %bb.51:
	tbnz	w17, #0, LBB97_60
; %bb.52:
	tbnz	w0, #0, LBB97_60
; %bb.53:
	tbnz	w1, #0, LBB97_60
; %bb.54:
	tbnz	w2, #0, LBB97_60
; %bb.55:
	ldr	x1, [sp, #48]                   ; 8-byte Folded Reload
	tbnz	w3, #0, LBB97_44
; %bb.56:
	tbnz	w4, #0, LBB97_44
; %bb.57:
	and	x16, x12, #0xfffffffffffffff8
	add	x14, x16, x15
	add	x15, x11, x15
	lsl	x15, x15, #2
	add	x15, x15, #16
	mov	x17, x8
	mov	x0, x9
	mov	x2, x16
LBB97_58:                               ; =>This Inner Loop Header: Depth=1
	add	x3, x0, x13
	ldp	q0, q1, [x3]
	add	x4, x1, x13
	ldp	q2, q3, [x4]
	add	x4, x17, x13
	ldp	q4, q5, [x4]
	fmla.4s	v0, v4, v2
	fmla.4s	v1, v5, v3
	add	x4, x1, x15
	ldp	q6, q7, [x4, #-16]
	add	x4, x17, x15
	ldp	q16, q17, [x4, #-16]
	fmls.4s	v0, v16, v6
	fmls.4s	v1, v17, v7
	add	x4, x0, x15
	ldp	q18, q19, [x4, #-16]
	fmla.4s	v18, v16, v2
	fmla.4s	v19, v17, v3
	fmla.4s	v18, v4, v6
	fmla.4s	v19, v5, v7
	stp	q0, q1, [x3]
	stp	q18, q19, [x4, #-16]
	add	x1, x1, #32
	add	x0, x0, #32
	add	x17, x17, #32
	subs	x2, x2, #8
	b.ne	LBB97_58
; %bb.59:
	cmp	x12, x16
	ldr	x1, [sp, #48]                   ; 8-byte Folded Reload
	b.ne	LBB97_45
	b	LBB97_47
LBB97_60:
	mov	x14, x15
	ldr	x1, [sp, #48]                   ; 8-byte Folded Reload
	b	LBB97_45
	.cfi_endproc
                                        