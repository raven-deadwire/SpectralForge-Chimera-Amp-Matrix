_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.weak_def_can_be_hidden	__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.p2align	2
__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb: ; @_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.cfi_startproc
; %bb.0:
	stp	x28, x27, [sp, #-96]!           ; 16-byte Folded Spill
	stp	x26, x25, [sp, #16]             ; 16-byte Folded Spill
	stp	x24, x23, [sp, #32]             ; 16-byte Folded Spill
	stp	x22, x21, [sp, #48]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #64]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #80]             ; 16-byte Folded Spill
	add	x29, sp, #80
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
	cbz	w3, LBB97_17
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
	ldr	w14, [x20, #32]
	cmp	w14, #2
	b.lt	LBB97_17
; %bb.4:
	ldr	x9, [x20, #64]
	sxtw	x10, w2
	ldr	w8, [x20, #28]
	lsl	w11, w8, #1
	smull	x8, w10, w11
	add	x8, x9, x8, lsl #2
	smull	x10, w11, w10
	add	x9, x9, x10, lsl #2
	lsr	x13, x2, #32
	mov	w10, #1                         ; =0x1
	mov	w11, #4                         ; =0x4
                                        ; kill: def $w13 killed $w13 killed $x13 def $x13
	b	LBB97_6
LBB97_5:                                ;   in Loop: Header=BB97_6 Depth=1
	add	x10, x10, #1
	ldrsw	x14, [x20, #32]
	add	x11, x11, #4
	cmp	x10, x14
	b.ge	LBB97_17
LBB97_6:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB97_10 Depth 2
                                        ;     Child Loop BB97_16 Depth 2
	mov	w1, #0                          ; =0x0
	add	w15, w13, #1
	cmp	w15, w14
	csinc	w13, wzr, w13, ge
	sxtw	x15, w13
	ldr	w0, [x20, #28]
	lsl	w2, w0, #1
	sxtw	x17, w2
	ldr	x14, [x20]
	ldr	x16, [x14, #32]
	ldr	x14, [x20, #88]
	cmp	w0, #4
	b.lt	LBB97_14
; %bb.7:                                ;   in Loop: Header=BB97_6 Depth=1
	lsl	x3, x0, #2
	add	x1, x14, x3
	cmp	x0, #7
	mov	w12, #7                         ; =0x7
	csel	x4, x0, x12, hi
	sub	x5, x4, #4
	cmp	x5, #4
	b.hs	LBB97_9
; %bb.8:                                ;   in Loop: Header=BB97_6 Depth=1
	mov	x4, #0                          ; =0x0
	b	LBB97_11
LBB97_9:                                ;   in Loop: Header=BB97_6 Depth=1
	mov	x6, #0                          ; =0x0
	mov	x4, #0                          ; =0x0
	lsl	x7, x5, #2
	and	x7, x7, #0xfffffffffffffff0
	add	x7, x7, #16
	and	x7, x7, #0xffffffffffffffe0
	smull	x21, w15, w17
	lsl	x22, x21, #2
	add	x21, x9, x22
	add	x23, x9, x3
	add	x22, x23, x22
	mul	x24, x11, x17
	add	x23, x16, x3
	add	x23, x23, x24
	add	x24, x16, x24
LBB97_10:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	add	x25, x21, x6
	add	x26, x22, x6
	ldr	q0, [x25]
	ldr	q1, [x26]
	add	x27, x24, x6
	ldr	q2, [x27]
	add	x28, x23, x6
	add	x30, x14, x6
	ldr	q3, [x28]
	ldr	q4, [x30]
	add	x12, x1, x6
	ldp	q5, q6, [x12]
	fmla.4s	v5, v3, v0
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v2, v1
	str	q4, [x30]
	str	q5, [x12]
	ldr	q0, [x25, #16]
	ldr	q1, [x26, #16]
	ldr	q2, [x27, #16]
	ldr	q3, [x28, #16]
	ldr	q4, [x30, #16]
	fmla.4s	v4, v2, v0
	fmla.4s	v6, v3, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v6, v2, v1
	str	q4, [x30, #16]
	str	q6, [x12, #16]
	add	x4, x4, #8
	add	x6, x6, #32
	cmp	x7, x6
	b.ne	LBB97_10
LBB97_11:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w5, #2, LBB97_13
; %bb.12:                               ;   in Loop: Header=BB97_6 Depth=1
	smull	x2, w2, w13
	add	x2, x8, x2, lsl #2
	mul	x5, x10, x17
	add	x5, x16, x5, lsl #2
	add	x6, x2, x3
	add	x3, x5, x3
	lsl	x4, x4, #2
	ldr	q0, [x2, x4]
	ldr	q1, [x6, x4]
	ldr	q2, [x5, x4]
	ldr	q3, [x3, x4]
	ldr	q4, [x14, x4]
	ldr	q5, [x1, x4]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x14, x4]
	str	q5, [x1, x4]
LBB97_13:                               ;   in Loop: Header=BB97_6 Depth=1
	and	w1, w0, #0x7ffffffc
LBB97_14:                               ;   in Loop: Header=BB97_6 Depth=1
	cmp	w1, w0
	b.ge	LBB97_5
; %bb.15:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x2, #0                          ; =0x0
	mov	w4, w1
	sub	x3, x0, x4
	add	x0, x4, x0
	lsl	x4, x0, #2
	add	x0, x14, x4
	mul	x5, x11, x17
	smull	x17, w15, w17
	add	x15, x16, x4
	add	x15, x15, x5
	lsl	x6, x17, #2
	add	x17, x9, x4
	add	x17, x17, x6
	ubfiz	x4, x1, #2, #32
	add	x16, x16, x4
	add	x16, x16, x5
	add	x1, x9, x4
	add	x1, x1, x6
	add	x14, x14, x4
LBB97_16:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	s0, [x14, x2, lsl #2]
	ldr	s1, [x1, x2, lsl #2]
	ldr	s2, [x16, x2, lsl #2]
	ldr	s3, [x17, x2, lsl #2]
	ldr	s4, [x15, x2, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x0, x2, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x14, x2, lsl #2]
	str	s1, [x0, x2, lsl #2]
	add	x2, x2, #1
	cmp	x3, x2
	b.ne	LBB97_16
	b	LBB97_5
LBB97_17:
	ldr	x0, [x20, #112]
	ldr	x1, [x20, #88]
	ldr	w8, [x20, #28]
	lsl	w2, w8, #1
	bl	__ZN4juce25FloatVectorOperationsBaseIfiE4copyEPfPKfi
	ldr	x8, [x20]
	ldr	x8, [x8, #32]
	ldr	x9, [x20, #112]
	ldr	w10, [x20, #28]
	lsl	x11, x10, #2
	cmp	w10, #4
	b.ge	LBB97_19
; %bb.18:
	mov	w14, #0                         ; =0x0
	cmp	w14, w10
	b.lt	LBB97_26
	b	LBB97_28
LBB97_19:
	mov	w12, #7                         ; =0x7
	cmp	x10, #7
	csel	x12, x10, x12, hi
	sub	x13, x12, #4
	cmp	x13, #4
	b.hs	LBB97_21
; %bb.20:
	mov	x12, #0                         ; =0x0
	b	LBB97_23
LBB97_21:
	mov	x12, #0                         ; =0x0
	and	x14, x13, #0xfffffffffffffffc
	add	x14, x14, #4
	and	x14, x14, #0xfffffffffffffff8
	add	x15, x19, #16
	add	x16, x8, #16
	add	x17, x9, #16
LBB97_22:                               ; =>This Inner Loop Header: Depth=1
	ldur	q0, [x15, #-16]
	add	x0, x15, x11
	ldur	q1, [x16, #-16]
	add	x1, x16, x11
	ldur	q2, [x1, #-16]
	ldur	q3, [x17, #-16]
	add	x2, x17, x11
	fmla.4s	v3, v1, v0
	ldp	q4, q5, [x2, #-16]
	fmla.4s	v4, v2, v0
	ldur	q0, [x0, #-16]
	fmls.4s	v3, v2, v0
	fmla.4s	v4, v1, v0
	stur	q3, [x17, #-16]
	stur	q4, [x2, #-16]
	ldr	q0, [x15], #32
	ldr	q1, [x0]
	ldr	q2, [x16], #32
	ldr	q3, [x1]
	ldr	q4, [x17]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	add	x12, x12, #8
	str	q4, [x17], #32
	str	q5, [x2]
	cmp	x14, x12
	b.ne	LBB97_22
LBB97_23:
	tbnz	w13, #2, LBB97_25
; %bb.24:
	add	x13, x19, x11
	add	x14, x8, x11
	add	x15, x9, x11
	lsl	x12, x12, #2
	ldr	q0, [x19, x12]
	ldr	q1, [x13, x12]
	ldr	q2, [x8, x12]
	ldr	q3, [x14, x12]
	ldr	q4, [x9, x12]
	ldr	q5, [x15, x12]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x9, x12]
	str	q5, [x15, x12]
LBB97_25:
	and	w14, w10, #0x7ffffffc
	cmp	w14, w10
	b.ge	LBB97_28
LBB97_26:
	add	x12, x9, x11
	add	x13, x8, x11
	add	x11, x19, x11
	mov	w14, w14
LBB97_27:                               ; =>This Inner Loop Header: Depth=1
	ldr	s0, [x9, x14, lsl #2]
	ldr	s1, [x19, x14, lsl #2]
	ldr	s2, [x8, x14, lsl #2]
	ldr	s3, [x11, x14, lsl #2]
	ldr	s4, [x13, x14, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x12, x14, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x9, x14, lsl #2]
	str	s1, [x12, x14, lsl #2]
	add	x14, x14, #1
	cmp	x10, x14
	b.ne	LBB97_27
LBB97_28:
	ldp	x29, x30, [sp, #80]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #64]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #48]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #16]             ; 16-byte Folded Reload
	ldp	x28, x27, [sp], #96             ; 16-byte Folded Reload
	ret
	.cfi_endproc
                                        