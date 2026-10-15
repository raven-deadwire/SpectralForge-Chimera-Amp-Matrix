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
	str	x1, [sp, #40]                   ; 8-byte Folded Spill
	mov	x20, x0
	cbz	w3, LBB97_31
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
	ldr	w0, [x20, #32]
	cmp	w0, #2
	b.lt	LBB97_31
; %bb.4:
	mov	x8, #0                          ; =0x0
	ldr	x9, [x20, #64]
	sxtw	x12, w2
	ldr	w10, [x20, #28]
	lsl	w13, w10, #1
	smull	x10, w12, w13
	stp	x10, x9, [sp, #24]              ; 16-byte Folded Spill
	add	x19, x9, x10, lsl #2
	smull	x12, w13, w12
	add	x9, x9, x12, lsl #2
	lsr	x17, x2, #32
	str	x9, [sp, #56]                   ; 8-byte Folded Spill
	add	x9, x9, #16
	stp	x9, x19, [sp, #8]               ; 16-byte Folded Spill
	mov	w14, #1                         ; =0x1
	mov	w15, #4                         ; =0x4
                                        ; kill: def $w17 killed $w17 killed $x17 def $x17
	b	LBB97_6
LBB97_5:                                ;   in Loop: Header=BB97_6 Depth=1
	add	x14, x14, #1
	ldrsw	x0, [x20, #32]
	add	x8, x8, #1
	add	x15, x15, #4
	cmp	x14, x0
	b.ge	LBB97_31
LBB97_6:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB97_10 Depth 2
                                        ;     Child Loop BB97_28 Depth 2
                                        ;     Child Loop BB97_18 Depth 2
	mov	w6, #0                          ; =0x0
	add	w1, w17, #1
	cmp	w1, w0
	csinc	w17, wzr, w17, ge
	sxtw	x1, w17
	ldr	w4, [x20, #28]
	lsl	w0, w4, #1
	sxtw	x3, w0
	smull	x5, w0, w17
	ldr	x0, [x20]
	ldr	x2, [x0, #32]
	ldr	x0, [x20, #88]
	cmp	w4, #4
	b.lt	LBB97_14
; %bb.7:                                ;   in Loop: Header=BB97_6 Depth=1
	add	x7, x19, x5, lsl #2
	lsl	x21, x4, #2
	add	x6, x0, x21
	cmp	x4, #7
	mov	w9, #7                          ; =0x7
	csel	x22, x4, x9, hi
	sub	x23, x22, #4
	cmp	x23, #4
	b.hs	LBB97_9
; %bb.8:                                ;   in Loop: Header=BB97_6 Depth=1
	mov	x22, #0                         ; =0x0
	b	LBB97_11
LBB97_9:                                ;   in Loop: Header=BB97_6 Depth=1
	mov	x24, #0                         ; =0x0
	mov	x22, #0                         ; =0x0
	lsl	x25, x23, #2
	and	x25, x25, #0xfffffffffffffff0
	add	x25, x25, #16
	and	x25, x25, #0xffffffffffffffe0
	smull	x26, w1, w3
	add	x26, x21, x26, lsl #2
	ldr	x9, [sp, #56]                   ; 8-byte Folded Reload
	add	x26, x9, x26
	mul	x28, x15, x3
	add	x27, x2, x28
	add	x30, x2, x21
	add	x28, x30, x28
LBB97_10:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	add	x9, x7, x24
	add	x10, x26, x24
	ldr	q0, [x9]
	ldr	q1, [x10]
	add	x11, x27, x24
	ldr	q2, [x11]
	add	x12, x28, x24
	add	x13, x0, x24
	ldr	q3, [x12]
	ldr	q4, [x13]
	add	x16, x6, x24
	ldp	q5, q6, [x16]
	fmla.4s	v5, v3, v0
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v2, v1
	str	q4, [x13]
	str	q5, [x16]
	ldr	q0, [x9, #16]
	ldr	q1, [x10, #16]
	ldr	q2, [x11, #16]
	ldr	q3, [x12, #16]
	ldr	q4, [x13, #16]
	fmla.4s	v4, v2, v0
	fmla.4s	v6, v3, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v6, v2, v1
	str	q4, [x13, #16]
	str	q6, [x16, #16]
	add	x22, x22, #8
	add	x24, x24, #32
	cmp	x25, x24
	b.ne	LBB97_10
LBB97_11:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w23, #2, LBB97_13
; %bb.12:                               ;   in Loop: Header=BB97_6 Depth=1
	mul	x23, x14, x3
	add	x23, x2, x23, lsl #2
	add	x24, x7, x21
	add	x21, x23, x21
	lsl	x22, x22, #2
	ldr	q0, [x7, x22]
	ldr	q1, [x24, x22]
	ldr	q2, [x23, x22]
	ldr	q3, [x21, x22]
	ldr	q4, [x0, x22]
	ldr	q5, [x6, x22]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x0, x22]
	str	q5, [x6, x22]
LBB97_13:                               ;   in Loop: Header=BB97_6 Depth=1
	and	w6, w4, #0x7ffffffc
LBB97_14:                               ;   in Loop: Header=BB97_6 Depth=1
	cmp	w6, w4
	b.ge	LBB97_5
; %bb.15:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	w7, w6
	sub	x6, x4, x7
	cmp	x6, #11
	b.hi	LBB97_19
LBB97_16:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x5, x7
LBB97_17:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x6, #0                          ; =0x0
	sub	x7, x4, x5
	add	x4, x5, x4
	lsl	x21, x4, #2
	add	x4, x0, x21
	mul	x22, x15, x3
	smull	x3, w1, w3
	add	x1, x2, x21
	add	x1, x1, x22
	lsl	x23, x3, #2
	ldr	x9, [sp, #56]                   ; 8-byte Folded Reload
	add	x3, x9, x21
	add	x3, x3, x23
	lsl	x21, x5, #2
	add	x2, x2, x21
	add	x2, x2, x22
	add	x5, x9, x21
	add	x5, x5, x23
	add	x0, x0, x21
LBB97_18:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	s0, [x0, x6, lsl #2]
	ldr	s1, [x5, x6, lsl #2]
	ldr	s2, [x2, x6, lsl #2]
	ldr	s3, [x3, x6, lsl #2]
	ldr	s4, [x1, x6, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x4, x6, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x0, x6, lsl #2]
	str	s1, [x4, x6, lsl #2]
	add	x6, x6, #1
	cmp	x7, x6
	b.ne	LBB97_18
	b	LBB97_5
LBB97_19:                               ;   in Loop: Header=BB97_6 Depth=1
	lsl	x21, x8, #2
	add	x21, x21, #4
	add	x22, x7, x4
	lsl	x24, x22, #2
	lsl	x22, x4, #3
	add	x28, x0, x22
	str	x1, [sp, #48]                   ; 8-byte Folded Spill
	smull	x23, w1, w3
	add	x25, x19, x23, lsl #2
	ldp	x10, x9, [sp, #24]              ; 16-byte Folded Reload
	add	x23, x9, x22
	add	x5, x10, x5
	add	x26, x5, x4
	add	x30, x23, x5, lsl #2
	lsl	x23, x7, #2
	add	x27, x25, x23
	add	x26, x9, x26, lsl #2
	madd	x13, x21, x3, x2
	add	x16, x13, x24
	add	x10, x13, x23
	lsl	x9, x4, #2
	add	x11, x0, x9
	add	x19, x0, x24
	cmp	x19, x26
	ccmp	x27, x28, #2, lo
	cset	w5, lo
	add	x12, x13, x22
	cmp	x19, x12
	ccmp	x16, x28, #2, lo
	cset	w21, lo
	add	x9, x13, x9
	cmp	x19, x9
	ccmp	x10, x28, #2, lo
	cset	w22, lo
	add	x13, x0, x23
	cmp	x13, x30
	add	x1, x25, x24
	ccmp	x1, x11, #2, lo
	cset	w24, lo
	cmp	x13, x26
	ccmp	x27, x11, #2, lo
	cset	w25, lo
	cmp	x13, x12
	ccmp	x16, x11, #2, lo
	cset	w26, lo
	cmp	x13, x9
	ccmp	x10, x11, #2, lo
	cset	w27, lo
	cmp	x1, x28
	ccmp	x19, x30, #2, lo
	b.lo	LBB97_30
; %bb.20:                               ;   in Loop: Header=BB97_6 Depth=1
	ldr	x19, [sp, #16]                  ; 8-byte Folded Reload
	ldr	x1, [sp, #48]                   ; 8-byte Folded Reload
	tbnz	w5, #0, LBB97_16
; %bb.21:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w21, #0, LBB97_16
; %bb.22:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w22, #0, LBB97_16
; %bb.23:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w24, #0, LBB97_16
; %bb.24:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w25, #0, LBB97_16
; %bb.25:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w26, #0, LBB97_16
; %bb.26:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w27, #0, LBB97_16
; %bb.27:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x22, #0                         ; =0x0
	and	x21, x6, #0xfffffffffffffff8
	add	x5, x21, x7
	smull	x9, w1, w3
	lsl	x9, x9, #2
	add	x10, x7, x4
	lsl	x10, x10, #2
	ldr	x11, [sp, #8]                   ; 8-byte Folded Reload
	add	x11, x11, x10
	add	x7, x11, x9
	add	x24, x0, x23
	mul	x11, x15, x3
	ldr	x12, [sp, #56]                  ; 8-byte Folded Reload
	add	x12, x12, x23
	add	x25, x12, x9
	add	x9, x2, x10
	add	x9, x9, x11
	add	x26, x9, #16
	add	x9, x0, x10
	add	x27, x9, #16
	add	x9, x2, x23
	add	x23, x9, x11
	mov	x28, x21
LBB97_28:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	add	x9, x24, x22
	ldp	q0, q1, [x9]
	add	x10, x25, x22
	ldp	q2, q3, [x10]
	add	x10, x23, x22
	ldp	q4, q5, [x10]
	fmla.4s	v0, v4, v2
	fmla.4s	v1, v5, v3
	add	x10, x7, x22
	ldp	q6, q7, [x10, #-16]
	add	x10, x26, x22
	ldp	q16, q17, [x10, #-16]
	fmls.4s	v0, v16, v6
	fmls.4s	v1, v17, v7
	add	x10, x27, x22
	ldp	q18, q19, [x10, #-16]
	fmla.4s	v18, v16, v2
	fmla.4s	v19, v17, v3
	fmla.4s	v18, v4, v6
	fmla.4s	v19, v5, v7
	stp	q0, q1, [x9]
	stp	q18, q19, [x10, #-16]
	add	x22, x22, #32
	subs	x28, x28, #8
	b.ne	LBB97_28
; %bb.29:                               ;   in Loop: Header=BB97_6 Depth=1
	cmp	x6, x21
	b.eq	LBB97_5
	b	LBB97_17
LBB97_30:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x5, x7
	ldr	x19, [sp, #16]                  ; 8-byte Folded Reload
	ldr	x1, [sp, #48]                   ; 8-byte Folded Reload
	b	LBB97_17
LBB97_31:
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
	b.ge	LBB97_33
; %bb.32:
	mov	w11, #0                         ; =0x0
	ldr	x6, [sp, #40]                   ; 8-byte Folded Reload
	cmp	w11, w10
	b.lt	LBB97_40
	b	LBB97_44
LBB97_33:
	lsl	x11, x10, #2
	mov	w12, #7                         ; =0x7
	cmp	x10, #7
	csel	x12, x10, x12, hi
	sub	x13, x12, #4
	cmp	x13, #4
	b.hs	LBB97_35
; %bb.34:
	mov	x12, #0                         ; =0x0
	ldr	x6, [sp, #40]                   ; 8-byte Folded Reload
	b	LBB97_37
LBB97_35:
	mov	x12, #0                         ; =0x0
	and	x14, x13, #0xfffffffffffffffc
	add	x14, x14, #4
	and	x14, x14, #0xfffffffffffffff8
	ldr	x6, [sp, #40]                   ; 8-byte Folded Reload
	add	x15, x6, #16
	add	x16, x8, #16
	add	x17, x9, #16
LBB97_36:                               ; =>This Inner Loop Header: Depth=1
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
	b.ne	LBB97_36
LBB97_37:
	tbnz	w13, #2, LBB97_39
; %bb.38:
	add	x13, x6, x11
	add	x14, x8, x11
	add	x11, x9, x11
	lsl	x12, x12, #2
	ldr	q0, [x6, x12]
	ldr	q1, [x13, x12]
	ldr	q2, [x8, x12]
	ldr	q3, [x14, x12]
	ldr	q4, [x9, x12]
	ldr	q5, [x11, x12]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x9, x12]
	str	q5, [x11, x12]
LBB97_39:
	and	w11, w10, #0x7ffffffc
	cmp	w11, w10
	b.ge	LBB97_44
LBB97_40:
	mov	w15, w11
	sub	x12, x10, x15
	lsl	x11, x10, #2
	cmp	x12, #15
	b.hi	LBB97_45
LBB97_41:
	mov	x14, x15
LBB97_42:
	add	x12, x9, x11
	add	x13, x8, x11
	add	x11, x6, x11
LBB97_43:                               ; =>This Inner Loop Header: Depth=1
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
	b.ne	LBB97_43
LBB97_44:
	ldp	x29, x30, [sp, #144]            ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #128]            ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #112]            ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #96]             ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #80]             ; 16-byte Folded Reload
	ldp	x28, x27, [sp, #64]             ; 16-byte Folded Reload
	add	sp, sp, #160
	ret
LBB97_45:
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
	b.lo	LBB97_56
; %bb.46:
	ldr	x6, [sp, #40]                   ; 8-byte Folded Reload
	tbnz	w14, #0, LBB97_41
; %bb.47:
	tbnz	w16, #0, LBB97_41
; %bb.48:
	tbnz	w17, #0, LBB97_41
; %bb.49:
	tbnz	w0, #0, LBB97_41
; %bb.50:
	tbnz	w1, #0, LBB97_41
; %bb.51:
	tbnz	w2, #0, LBB97_41
; %bb.52:
	tbnz	w3, #0, LBB97_41
; %bb.53:
	and	x16, x12, #0xfffffffffffffff8
	add	x14, x16, x15
	add	x15, x15, x10
	lsl	x15, x15, #2
	add	x15, x15, #16
	mov	x17, x8
	mov	x0, x9
	mov	x1, x6
	mov	x2, x16
LBB97_54:                               ; =>This Inner Loop Header: Depth=1
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
	b.ne	LBB97_54
; %bb.55:
	cmp	x12, x16
	b.ne	LBB97_42
	b	LBB97_44
LBB97_56:
	mov	x14, x15
	ldr	x6, [sp, #40]                   ; 8-byte Folded Reload
	b	LBB97_42
	.cfi_endproc
                                        