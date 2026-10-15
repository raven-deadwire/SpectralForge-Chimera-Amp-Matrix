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
	mov	x20, x0
	str	x1, [sp, #16]                   ; 8-byte Folded Spill
	tbz	w3, #0, LBB97_29
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
	ldp	w17, w0, [x20, #28]
	cmp	w0, #1
	b.le	LBB97_30
; %bb.4:
	mov	x8, #0                          ; =0x0
	ldr	x9, [x20, #64]
	sxtw	x12, w2
	lsl	w13, w17, #1
	smull	x10, w12, w13
	stp	x10, x9, [sp, #40]              ; 16-byte Folded Spill
	add	x11, x9, x10, lsl #2
	smull	x12, w13, w12
	add	x13, x9, x12, lsl #2
	lsr	x16, x2, #32
	add	x9, x13, #16
	str	x9, [sp, #8]                    ; 8-byte Folded Spill
	mov	w14, #1                         ; =0x1
	mov	w15, #4                         ; =0x4
                                        ; kill: def $w16 killed $w16 killed $x16 def $x16
	stp	x13, x11, [sp, #24]             ; 16-byte Folded Spill
	b	LBB97_6
LBB97_5:                                ;   in Loop: Header=BB97_6 Depth=1
	add	x14, x14, #1
	ldrsw	x0, [x20, #32]
	add	x8, x8, #1
	add	x15, x15, #4
	cmp	x14, x0
	b.ge	LBB97_30
LBB97_6:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB97_8 Depth 2
                                        ;     Child Loop BB97_23 Depth 2
                                        ;     Child Loop BB97_27 Depth 2
	add	w1, w16, #1
	cmp	w1, w0
	csinc	w16, wzr, w16, ge
	lsl	w0, w17, #1
	sxtw	x2, w0
	smull	x21, w0, w16
	ldr	x0, [x20]
	ldr	x1, [x0, #32]
	ldr	x0, [x20, #88]
	and	w4, w17, #0xfffffffc
	cmp	w4, #1
	b.lt	LBB97_10
; %bb.7:                                ;   in Loop: Header=BB97_6 Depth=1
	mov	x3, #0                          ; =0x0
	madd	x17, x15, x2, x1
	mov	x5, x0
	add	x6, x11, x21, lsl #2
LBB97_8:                                ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	q0, [x6]
	ldrsw	x9, [x20, #28]
	lsl	x9, x9, #2
	ldr	q1, [x6, x9]
	ldr	q2, [x17]
	ldr	q3, [x17, x9]
	ldr	q4, [x5]
	ldr	q5, [x5, x9]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x5]
	ldrsw	x9, [x20, #28]
	lsl	x9, x9, #2
	str	q5, [x5, x9]
	add	x3, x3, #4
	add	x5, x5, #16
	add	x17, x17, #16
	add	x6, x6, #16
	cmp	x3, x4
	b.lo	LBB97_8
; %bb.9:                                ;   in Loop: Header=BB97_6 Depth=1
	ldr	w17, [x20, #28]
	mov	w7, w3
	cmp	w7, w17
	b.ge	LBB97_5
	b	LBB97_11
LBB97_10:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x7, #0                          ; =0x0
	cmp	w7, w17
	b.ge	LBB97_5
LBB97_11:                               ;   in Loop: Header=BB97_6 Depth=1
	sxtw	x19, w16
	sxtw	x4, w17
	mov	w5, w17
	sub	x9, x5, x7
	cmp	x9, #11
	b.hi	LBB97_13
; %bb.12:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x21, x7
	b	LBB97_26
LBB97_13:                               ;   in Loop: Header=BB97_6 Depth=1
	str	x9, [sp, #56]                   ; 8-byte Folded Spill
	lsl	x22, x8, #2
	add	x23, x22, #4
	lsl	x22, x7, #2
	lsl	x24, x5, #2
	add	x25, x4, x7
	lsl	x25, x25, #2
	ldp	x9, x3, [sp, #40]               ; 16-byte Folded Reload
	add	x21, x9, x21
	add	x21, x21, x5
	add	x26, x3, x21, lsl #2
	add	x13, x0, x25
	add	x10, x0, x24
	add	x27, x4, x5
	lsl	x27, x27, #2
	add	x9, x0, x27
	stp	x4, x19, [sp, #64]              ; 16-byte Folded Spill
	smull	x28, w19, w2
	add	x28, x11, x28, lsl #2
	add	x30, x28, x22
	madd	x11, x23, x2, x1
	add	x12, x11, x22
	add	x28, x28, x25
	add	x21, x21, x4
	add	x19, x3, x21, lsl #2
	add	x3, x11, x25
	add	x6, x0, x22
	cmp	x6, x26
	ccmp	x30, x10, #2, lo
	cset	w21, lo
	add	x4, x11, x24
	cmp	x6, x4
	ccmp	x12, x10, #2, lo
	cset	w23, lo
	cmp	x6, x19
	ccmp	x28, x10, #2, lo
	cset	w24, lo
	add	x11, x11, x27
	cmp	x6, x11
	ccmp	x3, x10, #2, lo
	cset	w25, lo
	cmp	x13, x26
	ccmp	x30, x9, #2, lo
	cset	w26, lo
	cmp	x13, x4
	ccmp	x12, x9, #2, lo
	cset	w27, lo
	cmp	x13, x19
	ccmp	x28, x9, #2, lo
	cset	w28, lo
	cmp	x13, x11
	ccmp	x3, x9, #2, lo
	cset	w30, lo
	cmp	x13, x10
	ccmp	x6, x9, #2, lo
	b.lo	LBB97_28
; %bb.14:                               ;   in Loop: Header=BB97_6 Depth=1
	ldr	x13, [sp, #24]                  ; 8-byte Folded Reload
	ldp	x4, x19, [sp, #64]              ; 16-byte Folded Reload
	tbnz	w21, #0, LBB97_25
; %bb.15:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w23, #0, LBB97_25
; %bb.16:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w24, #0, LBB97_25
; %bb.17:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w25, #0, LBB97_25
; %bb.18:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w26, #0, LBB97_25
; %bb.19:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w27, #0, LBB97_25
; %bb.20:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w28, #0, LBB97_25
; %bb.21:                               ;   in Loop: Header=BB97_6 Depth=1
	tbnz	w30, #0, LBB97_25
; %bb.22:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x24, #0                         ; =0x0
	ldr	x3, [sp, #56]                   ; 8-byte Folded Reload
	and	x23, x3, #0xfffffffffffffff8
	add	x21, x23, x7
	smull	x9, w19, w2
	lsl	x9, x9, #2
	add	x10, x4, x7
	lsl	x10, x10, #2
	ldr	x11, [sp, #8]                   ; 8-byte Folded Reload
	add	x11, x11, x10
	add	x7, x11, x9
	add	x25, x0, x22
	mul	x11, x15, x2
	add	x12, x13, x22
	add	x26, x12, x9
	add	x9, x11, x10
	add	x9, x1, x9
	add	x27, x9, #16
	add	x9, x0, x10
	add	x28, x9, #16
	add	x9, x11, x22
	add	x22, x1, x9
	mov	x30, x23
LBB97_23:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	add	x9, x25, x24
	ldp	q0, q1, [x9]
	add	x10, x26, x24
	ldp	q2, q3, [x10]
	add	x10, x22, x24
	ldp	q4, q5, [x10]
	fmla.4s	v0, v4, v2
	fmla.4s	v1, v5, v3
	add	x10, x7, x24
	ldp	q6, q7, [x10, #-16]
	add	x10, x27, x24
	ldp	q16, q17, [x10, #-16]
	fmls.4s	v0, v16, v6
	fmls.4s	v1, v17, v7
	add	x10, x28, x24
	ldp	q18, q19, [x10, #-16]
	fmla.4s	v18, v16, v2
	fmla.4s	v19, v17, v3
	fmla.4s	v18, v4, v6
	fmla.4s	v19, v5, v7
	stp	q0, q1, [x9]
	stp	q18, q19, [x10, #-16]
	add	x24, x24, #32
	subs	x30, x30, #8
	b.ne	LBB97_23
; %bb.24:                               ;   in Loop: Header=BB97_6 Depth=1
	cmp	x3, x23
	ldr	x11, [sp, #32]                  ; 8-byte Folded Reload
	b.eq	LBB97_5
	b	LBB97_26
LBB97_25:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x21, x7
	ldr	x11, [sp, #32]                  ; 8-byte Folded Reload
LBB97_26:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x6, #0                          ; =0x0
	sub	x5, x5, x21
	add	x4, x21, x4
	lsl	x7, x4, #2
	add	x4, x0, x7
	mul	x22, x15, x2
	smull	x3, w19, w2
	add	x2, x22, x7
	add	x2, x1, x2
	lsl	x23, x3, #2
	add	x3, x13, x7
	add	x3, x3, x23
	lsl	x21, x21, #2
	add	x7, x22, x21
	add	x1, x1, x7
	add	x7, x13, x21
	add	x7, x7, x23
	add	x0, x0, x21
LBB97_27:                               ;   Parent Loop BB97_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ldr	s0, [x0, x6, lsl #2]
	ldr	s1, [x7, x6, lsl #2]
	ldr	s2, [x1, x6, lsl #2]
	ldr	s3, [x3, x6, lsl #2]
	ldr	s4, [x2, x6, lsl #2]
	fmadd	s0, s1, s2, s0
	fmsub	s0, s3, s4, s0
	ldr	s5, [x4, x6, lsl #2]
	fmadd	s1, s1, s4, s5
	fmadd	s1, s3, s2, s1
	str	s0, [x0, x6, lsl #2]
	str	s1, [x4, x6, lsl #2]
	add	x6, x6, #1
	cmp	x5, x6
	b.ne	LBB97_27
	b	LBB97_5
LBB97_28:                               ;   in Loop: Header=BB97_6 Depth=1
	mov	x21, x7
	ldp	x13, x11, [sp, #24]             ; 16-byte Folded Reload
	ldp	x4, x19, [sp, #64]              ; 16-byte Folded Reload
	b	LBB97_26
LBB97_29:
	ldr	w17, [x20, #28]
LBB97_30:
	ldr	x0, [x20, #112]
	ldr	x1, [x20, #88]
	lsl	w2, w17, #1
	bl	__ZN4juce25FloatVectorOperationsBaseIfiE4copyEPfPKfi
	ldr	x8, [x20]
	ldr	x8, [x8, #32]
	ldr	x9, [x20, #112]
	ldr	w12, [x20, #28]
	and	x11, x12, #0xfffffffc
	cmp	w11, #1
	b.lt	LBB97_34
; %bb.31:
	mov	x10, #0                         ; =0x0
	ldr	x24, [sp, #16]                  ; 8-byte Folded Reload
	mov	x12, x24
	mov	x13, x8
	mov	x14, x9
LBB97_32:                               ; =>This Inner Loop Header: Depth=1
	ldr	q0, [x12]
	ldrsw	x15, [x20, #28]
	lsl	x15, x15, #2
	ldr	q1, [x12, x15]
	ldr	q2, [x13]
	ldr	q3, [x13, x15]
	ldr	q4, [x14]
	ldr	q5, [x14, x15]
	fmla.4s	v4, v2, v0
	fmls.4s	v4, v3, v1
	fmla.4s	v5, v3, v0
	fmla.4s	v5, v2, v1
	str	q4, [x14]
	ldrsw	x15, [x20, #28]
	lsl	x15, x15, #2
	str	q5, [x14, x15]
	add	x10, x10, #4
	add	x14, x14, #16
	add	x13, x13, #16
	add	x12, x12, #16
	cmp	x10, x11
	b.lo	LBB97_32
; %bb.33:
	ldr	w12, [x20, #28]
	cmp	w10, w12
	b.lt	LBB97_35
	b	LBB97_39
LBB97_34:
	mov	w10, #0                         ; =0x0
                                        ; kill: def $w12 killed $w12 killed $x12 def $x12
	ldr	x24, [sp, #16]                  ; 8-byte Folded Reload
	cmp	w10, w12
	b.ge	LBB97_39
LBB97_35:
	mov	w15, w10
	sxtw	x11, w12
	mov	w10, w12
	sub	x12, x10, x15
	cmp	x12, #19
	b.hi	LBB97_40
LBB97_36:
	mov	x14, x15
LBB97_37:
	lsl	x13, x11, #2
	add	x11, x9, x13
	add	x12, x8, x13
	add	x13, x24, x13
LBB97_38:                               ; =>This Inner Loop Header: Depth=1
	ldr	s0, [x9, x14, lsl #2]
	ldr	s1, [x24, x14, lsl #2]
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
	b.ne	LBB97_38
LBB97_39:
	ldp	x29, x30, [sp, #160]            ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #144]            ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #128]            ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #112]            ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #96]             ; 16-byte Folded Reload
	ldp	x28, x27, [sp, #80]             ; 16-byte Folded Reload
	add	sp, sp, #176
	ret
LBB97_40:
	lsl	x13, x15, #2
	add	x5, x9, x13
	lsl	x16, x10, #2
	add	x14, x11, x15
	lsl	x14, x14, #2
	add	x1, x24, x16
	cmp	x5, x1
	add	x6, x9, x14
	add	x7, x9, x16
	add	x17, x11, x10
	lsl	x0, x17, #2
	add	x19, x9, x0
	add	x2, x24, x13
	add	x3, x8, x13
	add	x4, x24, x14
	add	x20, x8, x14
	ccmp	x2, x7, #2, lo
	cset	w14, lo
	add	x21, x8, x16
	cmp	x5, x21
	ccmp	x3, x7, #2, lo
	cset	w16, lo
	add	x22, x24, x0
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
	b.lo	LBB97_36
; %bb.41:
	tbnz	w14, #0, LBB97_36
; %bb.42:
	tbnz	w16, #0, LBB97_36
; %bb.43:
	tbnz	w17, #0, LBB97_36
; %bb.44:
	tbnz	w0, #0, LBB97_36
; %bb.45:
	tbnz	w1, #0, LBB97_36
; %bb.46:
	tbnz	w2, #0, LBB97_36
; %bb.47:
	tbnz	w3, #0, LBB97_36
; %bb.48:
	tbnz	w4, #0, LBB97_36
; %bb.49:
	and	x16, x12, #0xfffffffffffffff8
	add	x14, x16, x15
	add	x15, x11, x15
	lsl	x15, x15, #2
	add	x15, x15, #16
	mov	x17, x8
	mov	x0, x9
	mov	x1, x24
	mov	x2, x16
LBB97_50:                               ; =>This Inner Loop Header: Depth=1
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
	b.ne	LBB97_50
; %bb.51:
	cmp	x12, x16
	b.ne	LBB97_37
	b	LBB97_39
	.cfi_endproc
                                        