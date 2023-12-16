#pragma once
#include "Header.hpp"

namespace JIT
{
	Gp GetRegOfSize(Gp::Id id, int size);

	Gp GetContextReg();
	Gp GetTempReg(int size = 8);
	Gp GetTempReg2(int size = 8);
	/// <summary>
	/// Only safe when the stack is unstable
	/// </summary>
	Gp GetTempReg3(int size = 8);
	Gp GetStackExtensionReg(int size = 8);

	#define DEFINE_INSN_TYPE(name, func)                                                 \
	inline void name(Builder& a, const asmjit::Operand& dst, const asmjit::Operand& src, int size = 8) \
	{                                                                                    \
		if (dst.isMem() && src.isMem())                                                  \
		{                                                                                \
			a.mov(GetTempReg(size), src.as<Mem>());                                          \
			a.func(dst.as<Mem>(), GetTempReg(size));                                         \
		}                                                                                \
		else if (dst.isMem() && (src.isReg() || src.isImm()))                            \
		{                                                                                \
			if (src.isImm())                                                             \
				a.func(dst.as<Mem>(), src.as<asmjit::Imm>());                            \
			else                                                                         \
				a.func(dst.as<Mem>(), src.as<Gp>());                                     \
		}                                                                                \
		else if (dst.isReg() && src.isMem())                                             \
		{                                                                                \
			a.func(dst.as<Gp>(), src.as<Mem>());                                         \
		}                                                                                \
		else                                                                             \
		{                                                                                \
			if (src.isImm())                                                             \
				a.func(dst.as<Gp>(), src.as<asmjit::Imm>());                             \
			else                                                                         \
				a.func(dst.as<Gp>(), src.as<Gp>());                                      \
		}                                                                                \
	};
 
	#define DEFINE_INSN_TYPE_NOMEMDST(name, func)                                        \
	inline void name(Builder& a, const asmjit::Operand& dst, const asmjit::Operand& src) \
	{                                                                                    \
		if (dst.isMem() && src.isMem())                                                  \
		{                                                                                \
			a.mov(GetTempReg(), dst.as<Mem>());                                          \
			a.func(GetTempReg(), src.as<Mem>());                                         \
			a.mov(dst.as<Mem>(), GetTempReg());                                          \
		}                                                                                \
		else if (dst.isMem() && (src.isReg() || src.isImm()))                            \
		{                                                                                \
			if (src.isImm())                                                             \
			{                                                                            \
				a.mov(GetTempReg(), dst.as<Mem>());                                      \
				a.func(GetTempReg(), src.as<asmjit::Imm>());                             \
				a.mov(dst.as<Mem>(), GetTempReg());                                      \
            }                                                                            \
			else                                                                         \
            {                                                                            \
				a.mov(GetTempReg(), dst.as<Mem>());                                      \
				a.func(GetTempReg(), src.as<Gp>());                                      \
				a.mov(dst.as<Mem>(), GetTempReg());                                      \
            }                                                                            \
		}                                                                                \
		else if (dst.isReg() && src.isMem())                                             \
		{                                                                                \
			a.func(dst.as<Gp>(), src.as<Mem>());                                         \
		}                                                                                \
		else                                                                             \
		{                                                                                \
			if (src.isImm())                                                             \
				a.func(dst.as<Gp>(), src.as<asmjit::Imm>());                             \
			else                                                                         \
				a.func(dst.as<Gp>(), src.as<Gp>());                                      \
		}                                                                                \
	};

	void Load(Builder& a, Xmm reg, const asmjit::Operand& op);
	void Store(Builder& a, asmjit::Operand& op, Xmm reg);
	void CallExternal(Builder& a, const asmjit::Operand& op);
	
	#define CallDebug(a, func) \
	CallExternal(a, func);     \
	(a).cursor()->setInlineComment(#func);

	DEFINE_INSN_TYPE(Move, mov);
	DEFINE_INSN_TYPE(Add, add);
	DEFINE_INSN_TYPE(Sub, sub);
	DEFINE_INSN_TYPE(Compare, cmp);
	DEFINE_INSN_TYPE(And, and_);
	DEFINE_INSN_TYPE(Or, or_);
	DEFINE_INSN_TYPE(Xor, xor_);
	DEFINE_INSN_TYPE_NOMEMDST(IMul, imul);
}