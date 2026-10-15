_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.weak_def_can_be_hidden	__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.p2align	2
__ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb: ; @_ZN13spectralforge21ModeledCabConvolution13sumPartitionsEPKfNS0_9InputBankEb
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #160
	stp	x28, x27, [sp, #64]             ; 16-byte Folded Spill
	stp	x26, x25, [sp, #80]             ; 16-byte Folded Spill
	stp	x24, x23, [sp, #96]             ; 16-byte Folded Spill
	stp	x22, x21, [sp, #112]            ; 16-byte Folded Spill
	stp	x20, x19, [sp, #128]            ; 16-byte Folded Spill
	stp	x29, x30, [sp, #144]            ; 16-byte Folded Spill
	add	x29, sp, #144
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
	cbz	w3, LBB97_28
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
	ldr	w17, [x20, #32]
	cmp	w17, #2
	b.lt	LBB97_28
; %bb.4:
	mov	x8, #0                          ; =0x0
	ldr	x9, [x20, #64]
	sxtw	x12, w2
	ldr	w10, [x20, #28]
	lsl	w13, w10, #1
	smull	x10, w12, w13
	stp	x10, x9, [sp, #32]              ; 16-byte Folded Spill
	add	x10, x9, x10, lsl #2
	smull	x12, w13, w12
	add	x13, x9, x12, lsl #2
	lsr	x16, x2, #32
	add	x9, x13, #16
	stp	x9, x13, [sp, #8]               ; 16-byte Folded Spill
	mov	w14, #1                         ; =0x1
	mov	w15, #4                         ; =0x4
                                        ; kill: def $w16 killed $w16 killed $x16 def $x16
	str	x10, [sp, #24]                  ; 8-byte Folded Spill
	b	LBB97_6
LBB97_5:                                ;   in Loop: Header=BB97_6 Depth=1
	add	x14, x14, #1
	ldrsw	x17, [x20, #32]
	add	x8, x8, #1
	add	x15, x15, #4
	cmp	x14, x17
	b.ge	LBB97_28
LBB97_6:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB97_8 Depth 2
                                        ;     Child Loop BB97_22 Depth 2
                                        ;     Child Loop BB97_26 Depth 2
	mov	w4, #0                          ; =0x0
	add	w0, w16, #1
	cmp	w0, w17
	csinc	w16, wzr, w16, ge
	ldr	w2, [x20, #28]
	lsl	w17, w2, #1
	sxtw	x1, w17
	smull	x5, w17, w16
	ldr	x17, [x20]
	ldr	x0, [x17, #32]
	ldr	x17, [x20, #88]
	cmp	w2, #4
	b.lt	LBB97_10
; %bb.7:                                ;   in Loop: Header=BB97_6 Depth=1
	lsl	x3, x2, #2
	madd	x4, x15, x1, x0
	mov	x6, x17
	mov	w7, #4                          ; =0x4
	add	x21, x10, x5, lsl #2
LBB97_8:                                ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	q0, [x21]
	ldr	q1, [x21, x3]
	ldr	q2, [x4]
	ldr	q3, [x4, x3]
	ldr	q4, [x6]
	ldr	q5, [x6, x3]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x6]
	str	q5, [x6, x3]
	add	x7, x7, #4
	add	x6, x6, #16
	add	x4, x4, #16
	add	x21, x21, #16
	cmp	x7, x2
	b.ls	LBB97_8
; %bb.9:                                ;   in Loop: Header=BB97_6 Depth=1
	and	w4, w2, #0x7ffffffc
LBB97_10:                               ;   in Loop: Header=BB97_6 Depth=1
	cmp	w4, w2
	b.ge	LBB97_5
; %bb.11:                               ;   in Loop: Header=BB97_6 Depth=1
	sxtw	x3, w16
	mov	w6, w4
	sub	x4, x2, x6
	cmp	x4, #11
	b.hi	LBB97_13
; %bb.12:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x5, x6
	b	LBB97_25
LBB97_13:                               ;   in Loop: Header=BB97_6 Depth=1
	lsl	x7, x8, #2
	add	x7, x7, #4
	add	x21, x6, x2
	lsl	x23, x21, #2
	lsl	x21, x2, #3
	add	x27, x17, x21
	str	x3, [sp, #56]                   ; 8-byte Folded Spill
	smull	x22, w3, w1
	add	x24, x10, x22, lsl #2
	ldp	x10, x9, [sp, #32]              ; 16-byte Folded Reload
	add	x22, x9, x21
	add	x5, x10, x5
	add	x25, x5, x2
	add	x28, x22, x5, lsl #2
	lsl	x22, x6, #2
	add	x26, x24, x22
	add	x25, x9, x25, lsl #2
	madd	x30, x7, x1, x0
	add	x13, x30, x23
	add	x10, x30, x22
	lsl	x9, x2, #2
	add	x11, x17, x9
	add	x12, x17, x23
	cmp	x12, x25
	ccmp	x26, x27, #2, lo
	cset	w5, lo
	add	x19, x30, x21
	cmp	x12, x19
	ccmp	x13, x27, #2, lo
	cset	w7, lo
	add	x9, x30, x9
	cmp	x12, x9
	ccmp	x10, x27, #2, lo
	cset	w21, lo
	add	x30, x17, x22
	cmp	x30, x28
	add	x3, x24, x23
	ccmp	x3, x11, #2, lo
	cset	w23, lo
	cmp	x30, x25
	ccmp	x26, x11, #2, lo
	cset	w24, lo
	cmp	x30, x19
	ccmp	x13, x11, #2, lo
	cset	w25, lo
	cmp	x30, x9
	ccmp	x10, x11, #2, lo
	cset	w26, lo
	cmp	x3, x27
	ccmp	x12, x28, #2, lo
	b.lo	LBB97_27
; %bb.14:                               ;   in Loop: Header=BB97_6 Depth=1
	ldr	x13, [sp, #16]                  ; 8-byte Folded Reload
	ldr	x3, [sp, #56]                   ; 8-byte Folded Reload
	tbnz	w5, #0, LBB97_24
; %bb.15:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w7, #0, LBB97_24
; %bb.16:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w21, #0, LBB97_24
; %bb.17:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w23, #0, LBB97_24
; %bb.18:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w24, #0, LBB97_24
; %bb.19:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w25, #0, LBB97_24
; %bb.20:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w26, #0, LBB97_24
; %bb.21:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x21, #0                         ; =0x0
	and	x7, x4, #0xfffffffffffffff8
	add	x5, x7, x6
	smull	x9, w3, w1
	lsl	x9, x9, #2
	add	x10, x6, x2
	lsl	x10, x10, #2
	ldr	x11, [sp, #8]                   ; 8-byte Folded Reload
	add	x11, x11, x10
	add	x6, x11, x9
	add	x23, x17, x22
	mul	x11, x15, x1
	add	x12, x13, x22
	add	x24, x12, x9
	add	x9, x0, x10
	add	x9, x9, x11
	add	x25, x9, #16
	add	x9, x17, x10
	add	x26, x9, #16
	add	x9, x0, x22
	add	x22, x9, x11
	mov	x27, x7
LBB97_22:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	add	x9, x23, x21
	ldp	q0, q1, [x9]
	add	x10, x24, x21
	ldp	q2, q3, [x10]
	add	x10, x22, x21
	ldp	q4, q5, [x10]
	fmla.4s	v0, v4, v2
	fmla.4s	v1, v5, v3
	add	x10, x6, x21
	ldp	q6, q7, [x10, #-16]
	add	x10, x25, x21
	ldp	q16, q17, [x10, #-16]
	fmls.4s	v0, v16, v6
	fmls.4s	v1, v17, v7
	add	x10, x26, x21
	ldp	q18, q19, [x10, #-16]
	fmla.4s	v18, v16, v2
	fmla.4s	v19, v17, v3
	fmla.4s	v18, v4, v6
	fmla.4s	v19, v5, v7
	stp	q0, q1, [x9]
	stp	q18, q19, [x10, #-16]
	add	x21, x21, #32
	subs	x27, x27, #8
	b.ne	LBB97_22
; %bb.23:                               ;   in Loop: Header=BB97_6 Depth=1
	cmp	x4, x7
	ldr	x10, [sp, #24]                  ; 8-byte Folded Reload
	b.eq	LBB97_5
	b	LBB97_25
LBB97_24:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x5, x6
	ldr	x10, [sp, #24]                  ; 8-byte Folded Reload
LBB97_25:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x4, #0                          ; =0x0
	sub	x6, x2, x5
	add	x2, x5, x2
	lsl	x7, x2, #2
	add	x2, x17, x7
	mul	x21, x15, x1
	smull	x3, w3, w1
	add	x1, x0, x7
	add	x1, x1, x21
	lsl	x22, x3, #2
	add	x3, x13, x7
	add	x3, x3, x22
	lsl	x7, x5, #2
	add	x0, x0, x7
	add	x0, x0, x21
	add	x5, x13, x7
	add	x5, x5, x22
	add	x17, x17, x7
LBB97_26:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	s0, [x17, x4, lsl #2]
	ldr	s1, [x5, x4, lsl #2]
	ldr	s2, [x0, x4, lsl #2]
	ldr	s3, [x3, x4, lsl #2]
	ldr	s4, [x1, x4, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x2, x4, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x17, x4, lsl #2]
	str	s1, [x2, x4, lsl #2]
	add	x4, x4, #1
	cmp	x6, x4
	b.ne	LBB97_26
	b	LBB97_5
LBB97_27:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x5, x6
	ldp	x13, x10, [sp, #16]             ; 16-byte Folded Reload
	ldr	x3, [sp, #56]                   ; 8-byte Folded Reload
	b	LBB97_25
LBB97_28:
	ldr	x0, [x20, #112]
	ldr	x1, [x20, #88]
	ldr	w8, [x20, #28]
	lsl	w2, w8, #1
	bl	__ZN4juce25FloatVectorOperationsBaseIfiE4copyEPfPKfi
	ldr	x8, [x20]
	ldr	x8, [x8, #32]
	ldr	x9, [x20, #112]
	ldr	w10, [x20, #28]
	cmp	w10, #4
	b.ge	LBB97_30
; %bb.29:
	mov	w11, #0                         ; =0x0
	ldr	x6, [sp, #48]                   ; 8-byte Folded Reload
	cmp	w11, w10
	b.lt	LBB97_33
	b	LBB97_37
LBB97_30:
	lsl	x11, x10, #2
	mov	w12, #4                         ; =0x4
	ldr	x6, [sp, #48]                   ; 8-byte Folded Reload
	mov	x13, x6
	mov	x14, x8
	mov	x15, x9
LBB97_31:                               ; =>This Inner Loop Header: Depth=1
	ldr	q0, [x13]
	ldr	q1, [x13, x11]
	ldr	q2, [x14]
	ldr	q3, [x14, x11]
	ldr	q4, [x15]
	ldr	q5, [x15, x11]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x15]
	str	q5, [x15, x11]
	add	x12, x12, #4
	add	x15, x15, #16
	add	x14, x14, #16
	add	x13, x13, #16
	cmp	x12, x10
	b.ls	LBB97_31
; %bb.32:
	and	w11, w10, #0x7ffffffc
	cmp	w11, w10
	b.ge	LBB97_37
LBB97_33:
	mov	w15, w11
	sub	x12, x10, x15
	lsl	x11, x10, #2
	cmp	x12, #15
	b.hi	LBB97_38
LBB97_34:
	mov	x14, x15
LBB97_35:
	add	x12, x9, x11
	add	x13, x8, x11
	add	x11, x6, x11
LBB97_36:                               ; =>This Inner Loop Header: Depth=1
	ldr	s0, [x9, x14, lsl #2]
	ldr	s1, [x6, x14, lsl #2]
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
	b.ne	LBB97_36
LBB97_37:
	ldp	x29, x30, [sp, #144]            ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #128]            ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #112]            ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #96]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #80]             ; 16-byte Folded Reload
	ldp	x28, x27, [sp, #64]             ; 16-byte Folded Reload
	add	sp, sp, #160
	ret
LBB97_38:
	add	x13, x15, x10
	lsl	x0, x13, #2
	add	x4, x9, x0
	lsl	x16, x10, #3
	add	x5, x9, x16
	mov	x23, x6
	add	x6, x6, x16
	lsl	x13, x15, #2
	add	x1, x23, x13
	add	x2, x8, x0
	add	x3, x8, x13
	add	x7, x9, x11
	add	x19, x23, x11
	cmp	x4, x19
	ccmp	x1, x5, #2, lo
	cset	w14, lo
	add	x20, x8, x16
	cmp	x4, x20
	ccmp	x2, x5, #2, lo
	cset	w16, lo
	add	x21, x8, x11
	cmp	x4, x21
	ccmp	x3, x5, #2, lo
	cset	w17, lo
	add	x22, x9, x13
	cmp	x22, x6
	add	x23, x23, x0
	ccmp	x23, x7, #2, lo
	cset	w0, lo
	cmp	x22, x19
	ccmp	x1, x7, #2, lo
	cset	w1, lo
	cmp	x22, x20
	ccmp	x2, x7, #2, lo
	cset	w2, lo
	cmp	x22, x21
	ccmp	x3, x7, #2, lo
	cset	w3, lo
	cmp	x23, x5
	ccmp	x4, x6, #2, lo
	b.lo	LBB97_49
; %bb.39:
	ldr	x6, [sp, #48]                   ; 8-byte Folded Reload
	tbnz	w14, #0, LBB97_34
; %bb.40:
	tbnz	w16, #0, LBB97_34
; %bb.41:
	tbnz	w17, #0, LBB97_34
; %bb.42:
	tbnz	w0, #0, LBB97_34
; %bb.43:
	tbnz	w1, #0, LBB97_34
; %bb.44:
	tbnz	w2, #0, LBB97_34
; %bb.45:
	tbnz	w3, #0, LBB97_34
; %bb.46:
	and	x16, x12, #0xfffffffffffffff8
	add	x14, x16, x15
	add	x15, x15, x10
	lsl	x15, x15, #2
	add	x15, x15, #16
	mov	x17, x8
	mov	x0, x9
	mov	x1, x6
	mov	x2, x16
LBB97_47:                               ; =>This Inner Loop Header: Depth=1
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
	b.ne	LBB97_47
; %bb.48:
	cmp	x12, x16
	b.ne	LBB97_35
	b	LBB97_37
LBB97_49:
	mov	x14, x15
	ldr	x6, [sp, #48]                   ; 8-byte Folded Reload
	b	LBB97_35
	.cfi_endproc
                                        