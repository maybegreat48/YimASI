#include "Utils.hpp"
#include "Header.hpp"

JIT::Gp JIT::GetRegOfSize(Gp::Id id, int size)
{
	switch (size)
	{
	case 1: return GpbLo(id);
	case 2: return Gpw(id);
	case 4: return Gpd(id);
	case 8: return Gpq(id);
	}

	throw std::runtime_error("Invalid register size");
}

JIT::Gp JIT::GetContextReg()
{
	return rax;
}

JIT::Gp JIT::GetTempReg(int size)
{
	return GetRegOfSize(Gp::Id::kIdBx, size);
}

JIT::Gp JIT::GetTempReg2(int size)
{
	return GetRegOfSize(Gp::Id::kIdDx, size);
}

JIT::Gp JIT::GetTempReg3(int size)
{
	return GetRegOfSize(Gp::Id::kIdR9, size);
}

JIT::Gp JIT::GetStackExtensionReg(int size)
{
	return GetRegOfSize(Gp::Id::kIdR8, size);
}

void JIT::Load(Builder& a, Xmm reg, const asmjit::Operand& op)
{
	if (op.isReg())
		a.movd(reg, op.as<Gp>());
	else
		a.movss(reg, op.as<Mem>());
}

void JIT::Store(Builder& a, asmjit::Operand& op, Xmm reg)
{
	if (op.isReg())
		a.movd(op.as<Gp>(), reg);
	else
		a.movss(op.as<Mem>(), reg);
}

void JIT::CallExternal(Builder& a, const asmjit::Operand& op)
{
	if (ALLOCATE_SHADOW_SPACE)
		a.sub(rsp, 32);

	if (op.isImm())
		a.call(op.as<asmjit::Imm>());
	else if (op.isReg())
		a.call(op.as<Gp>());

	if (ALLOCATE_SHADOW_SPACE)
		a.add(rsp, 32);
}
