_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.weak_def_can_be_hidden	__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.p2align	2
__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb: ; @_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #128
	stp	x28, x27, [sp, #32]             ; 16-byte Folded Spill
	stp	x26, x25, [sp, #48]             ; 16-byte Folded Spill
	stp	x24, x23, [sp, #64]             ; 16-byte Folded Spill
	stp	x22, x21, [sp, #80]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #96]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #112]            ; 16-byte Folded Spill
	add	x29, sp, #112
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
	mov	x19, x1
	mov	x20, x0
	ldr	x1, [x0, #88]
	tbz	w3, #0, LBB97_10
; %bb.1:
	mov	x21, x2
	ldr	x8, [x20, #96]
	sub	x8, x8, x1
	cmp	x8, #1
	b.lt	LBB97_3
; %bb.2:
	mov	x0, x1
	mov	x22, x1
	mov	x1, x8
	bl	_bzero
	mov	x1, x22
LBB97_3:
	ldp	w14, w8, [x20, #28]
	lsl	w2, w14, #1
	cmp	w8, #1
	b.le	LBB97_11
; %bb.4:
	cmp	w14, #1
	b.lt	LBB97_11
; %bb.5:
	lsr	x9, x21, #32
                                        ; kill: def $w9 killed $w9 killed $x9 def $x9
	ldr	x3, [x20, #64]
	sxtw	x7, w21
	sxtw	x10, w2
	smull	x11, w7, w2
	add	x11, x3, x11, lsl #2
	ldr	x12, [x20]
	ldr	x24, [x12, #32]
	lsl	x22, x14, #2
	add	x12, x1, x22
	lsl	x4, x14, #3
	add	x15, x1, x4
	add	x13, x3, x22
	add	x0, x24, x10, lsl #2
	add	x16, x0, x22
	lsl	x17, x8, #2
	sub	x17, x17, #4
	madd	x5, x17, x10, x24
	add	x6, x5, x4
	cmp	x12, x6
	ccmp	x16, x15, #2, lo
	ccmp	w2, #0, #8, hs
	cset	w16, lt
	str	w16, [sp, #28]                  ; 4-byte Folded Spill
	cmp	x1, x5
	ccmp	x0, x15, #2, lo
	cset	w17, lo
	cmp	x1, x6
	ccmp	x0, x1, #2, lo
	cset	w6, lo
	add	x5, x5, x22
	cmp	x1, x5
	ccmp	x0, x12, #2, lo
	cset	w26, lo
	cmp	w14, #12
	b.hs	LBB97_28
; %bb.6:
	lsl	x14, x10, #2
	add	x15, x14, x22
	add	x15, x24, x15
	lsl	x16, x7, #2
	add	x17, x24, x14
	mov	w0, #1                          ; =0x1
LBB97_7:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB97_8 Depth 2
	mov	x3, #0                          ; =0x0
	add	w4, w9, #1
	cmp	w4, w8
	csinc	w9, wzr, w9, ge
	smull	x4, w10, w9
	add	x4, x11, x4, lsl #2
	add	x5, x16, w9, sxtw #2
	madd	x5, x10, x5, x13
LBB97_8:                                ;   Parent Loop BB97_7 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	s0, [x1, x3]
	ldr	s1, [x4, x3]
	ldr	s2, [x17, x3]
	ldr	s3, [x5, x3]
	ldr	s4, [x15, x3]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x12, x3]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x1, x3]
	str	s1, [x12, x3]
	add	x3, x3, #4
	cmp	x22, x3
	b.ne	LBB97_8
; %bb.9:                                ;   in Loop: Header=BB97_7 Depth=1
	add	x0, x0, #1
	add	x15, x15, x14
	add	x17, x17, x14
	cmp	x0, x8
	b.ne	LBB97_7
	b	LBB97_11
LBB97_10:
	ldr	w8, [x20, #28]
	lsl	w2, w8, #1
LBB97_11:
	ldr	x0, [x20, #112]
                                        ; kill: def $w2 killed $w2 killed $x2
	bl	__ZN4juce25FloatVectorOperationsBaseIfiE4copyEPfPKfi
	ldr	w8, [x20, #28]
	cmp	w8, #1
	b.lt	LBB97_16
LBB97_12:
	ldr	x9, [x20]
	ldr	x9, [x9, #32]
	ldr	x10, [x20, #112]
	lsl	x12, x8, #2
	cmp	w8, #16
	b.hs	LBB97_17
; %bb.13:
	mov	x11, #0                         ; =0x0
LBB97_14:
	add	x13, x10, x12
	add	x14, x9, x12
	add	x12, x19, x12
LBB97_15:                               ; =>This Inner Loop Header: Depth=1
	ldr	s0, [x10, x11, lsl #2]
	ldr	s1, [x19, x11, lsl #2]
	ldr	s2, [x9, x11, lsl #2]
	ldr	s3, [x12, x11, lsl #2]
	ldr	s4, [x14, x11, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x13, x11, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x10, x11, lsl #2]
	str	s1, [x13, x11, lsl #2]
	add	x11, x11, #1
	cmp	x8, x11
	b.ne	LBB97_15
LBB97_16:
	ldp	x29, x30, [sp, #112]            ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #96]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #80]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #64]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #48]             ; 16-byte Folded Reload
	ldp	x28, x27, [sp, #32]             ; 16-byte Folded Reload
	add	sp, sp, #128
	ret
LBB97_17:
	mov	x11, #0                         ; =0x0
	add	x2, x10, x12
	lsl	x15, x8, #3
	add	x1, x9, x12
	add	x3, x19, x15
	cmp	x10, x3
	cset	w13, lo
	cmp	x10, x19
	cset	w14, lo
	csel	w13, wzr, w13, ls
	add	x4, x10, x15
	cmp	x4, x19
	csel	w14, wzr, w14, ls
	add	x0, x9, x15
	cmp	x2, x0
	ccmp	x1, x4, #2, lo
	cset	w15, lo
	cmp	x10, x9
	ccmp	x9, x4, #2, lo
	cset	w16, lo
	add	x5, x19, x12
	cmp	x10, x5
	ccmp	x2, x19, #0, lo
	cset	w17, hi
	cmp	x10, x0
	ccmp	x9, x10, #2, lo
	cset	w0, lo
	cmp	x10, x1
	ccmp	x9, x2, #2, lo
	cset	w1, lo
	cmp	x5, x4
	ccmp	x2, x3, #2, lo
	b.lo	LBB97_14
; %bb.18:
	tbnz	w14, #0, LBB97_14
; %bb.19:
	tbnz	w15, #0, LBB97_14
; %bb.20:
	tbnz	w16, #0, LBB97_14
; %bb.21:
	tbnz	w13, #0, LBB97_14
; %bb.22:
	tbnz	w17, #0, LBB97_14
; %bb.23:
	tbnz	w0, #0, LBB97_14
; %bb.24:
	tbnz	w1, #0, LBB97_14
; %bb.25:
	and	x11, x8, #0x7ffffff8
	add	x13, x10, #16
	add	x14, x19, #16
	add	x15, x9, #16
	mov	x16, x11
LBB97_26:                               ; =>This Inner Loop Header: Depth=1
	ldp	q0, q1, [x13, #-16]
	ldp	q2, q3, [x14, #-16]
	ldp	q4, q5, [x15, #-16]
	fmla.4s	v0, v4, v2
	fmla.4s	v1, v5, v3
	add	x17, x14, x12
	ldp	q6, q7, [x17, #-16]
	add	x17, x15, x12
	ldp	q16, q17, [x17, #-16]
	fmls.4s	v0, v16, v6
	fmls.4s	v1, v17, v7
	add	x17, x13, x12
	ldp	q18, q19, [x17, #-16]
	fmla.4s	v18, v16, v2
	fmla.4s	v19, v17, v3
	fmla.4s	v18, v4, v6
	fmla.4s	v19, v5, v7
	stp	q0, q1, [x13, #-16]
	stp	q18, q19, [x17, #-16]
	add	x13, x13, #32
	add	x14, x14, #32
	add	x15, x15, #32
	subs	x16, x16, #8
	b.ne	LBB97_26
; %bb.27:
	cmp	x11, x8
	b.ne	LBB97_14
	b	LBB97_16
LBB97_28:
	lsl	x0, x21, #32
	asr	x0, x0, #30
	add	x3, x3, x4
	and	x4, x14, #0x7ffffff8
	ubfx	x5, x14, #3, #28
	lsl	x5, x5, #5
	add	x16, x13, #16
	stp	x2, x16, [sp, #8]               ; 16-byte Folded Spill
	lsl	x7, x7, #2
	lsl	x21, x10, #2
	add	x22, x21, x22
	add	x22, x24, x22
	add	x23, x22, #16
	add	x24, x24, x21
	mov	w25, #1                         ; =0x1
	orr	w26, w26, w6
	b	LBB97_30
LBB97_29:                               ;   in Loop: Header=BB97_30 Depth=1
	add	x25, x25, #1
	add	x23, x23, x21
	add	x24, x24, x21
	add	x22, x22, x21
	cmp	x25, x8
	b.eq	LBB97_37
LBB97_30:                               ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB97_33 Depth 2
                                        ;     Child Loop BB97_36 Depth 2
	add	w6, w9, #1
	cmp	w6, w8
	csinc	w9, wzr, w9, ge
	sxtw	x28, w9
	smull	x6, w10, w9
	add	x27, x11, x6, lsl #2
	add	x6, x0, w9, sxtw #2
	mul	x6, x6, x10
	add	x30, x3, x6
	cmp	x12, x30
	add	x6, x13, x6
	ccmp	x6, x15, #2, lo
	cset	w16, lo
	cmp	x12, x6
	ccmp	x27, x15, #2, lo
	csinc	w16, w16, wzr, hs
	ldr	w2, [sp, #28]                   ; 4-byte Folded Reload
	orr	w16, w16, w2
	orr	w16, w17, w16
	cmp	x1, x30
	ccmp	x6, x12, #2, lo
	csinc	w16, w16, wzr, hs
	cmp	x1, x6
	ccmp	x27, x12, #2, lo
	csinc	w16, w16, wzr, hs
	orr	w16, w26, w16
	tbz	w16, #0, LBB97_32
; %bb.31:                               ;   in Loop: Header=BB97_30 Depth=1
	mov	x6, #0                          ; =0x0
	b	LBB97_35
LBB97_32:                               ;   in Loop: Header=BB97_30 Depth=1
	mov	x30, #0                         ; =0x0
	add	x16, x7, x28, lsl #2
	ldr	x2, [sp, #16]                   ; 8-byte Folded Reload
	madd	x6, x10, x16, x2
LBB97_33:                               ;   Parent Loop BB97_30 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	add	x16, x1, x30
	ldp	q0, q1, [x16]
	add	x2, x27, x30
	ldp	q2, q3, [x2]
	add	x2, x24, x30
	ldp	q4, q5, [x2]
	fmla.4s	v0, v4, v2
	fmla.4s	v1, v5, v3
	add	x2, x6, x30
	ldp	q6, q7, [x2, #-16]
	add	x2, x23, x30
	ldp	q16, q17, [x2, #-16]
	fmls.4s	v0, v16, v6
	fmls.4s	v1, v17, v7
	add	x2, x12, x30
	ldp	q18, q19, [x2]
	fmla.4s	v18, v16, v2
	fmla.4s	v19, v17, v3
	fmla.4s	v18, v4, v6
	fmla.4s	v19, v5, v7
	stp	q0, q1, [x16]
	stp	q18, q19, [x2]
	add	x30, x30, #32
	cmp	x5, x30
	b.ne	LBB97_33
; %bb.34:                               ;   in Loop: Header=BB97_30 Depth=1
	mov	x6, x4
	cmp	x4, x14
	b.eq	LBB97_29
LBB97_35:                               ;   in Loop: Header=BB97_30 Depth=1
	add	x16, x7, x28, lsl #2
	madd	x28, x10, x16, x13
LBB97_36:                               ;   Parent Loop BB97_30 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	s0, [x1, x6, lsl #2]
	ldr	s1, [x27, x6, lsl #2]
	ldr	s2, [x24, x6, lsl #2]
	ldr	s3, [x28, x6, lsl #2]
	ldr	s4, [x22, x6, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x12, x6, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x1, x6, lsl #2]
	str	s1, [x12, x6, lsl #2]
	add	x6, x6, #1
	cmp	x14, x6
	b.ne	LBB97_36
	b	LBB97_29
LBB97_37:
	ldr	x2, [sp, #8]                    ; 8-byte Folded Reload
	ldr	x0, [x20, #112]
                                        ; kill: def $w2 killed $w2 killed $x2
	bl	__ZN4juce25FloatVectorOperationsBaseIfiE4copyEPfPKfi
	ldr	w8, [x20, #28]
	cmp	w8, #1
	b.ge	LBB97_12
	b	LBB97_16
	.cfi_endproc
                                        