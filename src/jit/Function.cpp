#include "Function.hpp"

#include "Instruction.hpp"
#include "ScriptFile.hpp"
#include "Utils.hpp"
#include "Context.hpp"
#include "Natives.hpp"

#include "pointers/Pointers.hpp"

#include <script/scrThread.hpp>

namespace
{
	using namespace asmjit::x86;

	bool IsJumpInstruction(JIT::Opcode op)
	{
		return op >= JIT::Opcode::J && op <= JIT::Opcode::ILE_JZ;
	}

	asmjit::Operand GetStackRegister(int index, int size = 8)
	{
		switch (index)
		{
		case 0: return JIT::GetRegOfSize(Gp::Id::kIdBp, size);
		case 1: return JIT::GetRegOfSize(Gp::Id::kIdSi, size);
		case 2: return JIT::GetRegOfSize(Gp::Id::kIdDi, size);
		case 3: return JIT::GetRegOfSize(Gp::Id::kIdR8, size);
		case 4: return JIT::GetRegOfSize(Gp::Id::kIdR9, size);
		case 5: return JIT::GetRegOfSize(Gp::Id::kIdR10, size);
		case 6: return JIT::GetRegOfSize(Gp::Id::kIdR11, size);
		case 7: return JIT::GetRegOfSize(Gp::Id::kIdR12, size);
		case 8: return JIT::GetRegOfSize(Gp::Id::kIdR13, size);
		case 9: return JIT::GetRegOfSize(Gp::Id::kIdR14, size);
		case 10: return JIT::GetRegOfSize(Gp::Id::kIdR15, size);
		}

		throw std::runtime_error("register index out of bounds");
	}

	inline const char* IntToString(char* buf, int val, int size = 20)
	{
		int index = size;
		bool neg  = false;
		if (val < 0)
		{
			neg = true;
			val = -val;
		}

		do
		{
			buf[--index] = (val % 10) + '0';
			val /= 10;
		} while (val);

		if (neg)
		{
			buf[--index] = '-';
		}

		return &buf[index];
	}

	void TEXT_LABEL_ASSIGN_STRING(char* dst, char* src, int len)
	{
		if (src)
			strncpy(dst, src, len);
		else
			*dst = 0;
	}

	void TEXT_LABEL_ASSIGN_INT(char* dst, int num, int len)
	{
		char buf[32]{};
		const char* src = IntToString(buf, num);
		strncpy(dst, src, len);
	}

	void TEXT_LABEL_APPEND_STRING(char* dst, char* src, int len)
	{
		strncat(dst, src, len);
	}

	void TEXT_LABEL_APPEND_INT(char* dst, int num, int len)
	{
		char buf[32]{};
		const char* src = IntToString(buf, num);
		strncat(dst, src, len);
	}

	struct CallIndirectResult
	{
		void* Function;
		uint64_t NumParams;
		uint64_t NumReturns;
		uint64_t ParamSize;
		uint64_t ReturnSize;
		uint64_t StableStack;
		uint64_t FakeStack;
	};

	static CallIndirectResult s_Result{};
	CallIndirectResult* CALLINDIRECT(JIT::Context* context, int index)
	{
		if (!index)
		{
			LOG(FATAL) << __FUNCTION__ ": index is null";
			s_Result.Function = nullptr;
			return &s_Result;
		}

		auto func = context->GetScriptFile()->GetFunctionAtAddress(index);

		if (!func)
		{
			LOG(FATAL) << __FUNCTION__ ": cannot find function at " << index;
			s_Result.Function = nullptr;
			return &s_Result;
		}

		s_Result.NumParams = func->NumParams;
		s_Result.NumReturns = func->NumReturns;
		s_Result.ParamSize  = func->NumParams * 8;
		s_Result.ReturnSize  = func->NumReturns * 8;
		s_Result.Function   = ((uint8_t*)context->GetScriptFile()->GetMainFunction()) + context->GetScriptFile()->GetCode().labelOffsetFromBase(func->FunctionLabel); 
		LOG(VERBOSE) << __FUNCTION__ ": resolved " << index << " to " << HEX(s_Result.Function);
		return &s_Result;
	}
}

void JIT::Function::MarkPushHandled(ConstantPushData& data)
{
	if (data.Index == -1)
		StackSnapshots[data.RelAddr].Skip = true;
	else
		StackSnapshots[data.RelAddr].SuppressPushes[data.Index] = true;
}

void JIT::Function::ProcessStackHills()
{
	bool inHill      = false;
	bool goodHill    = true;
	std::unordered_set<int> hillNodes{};
	int highestPeak = 0;

	for (auto& [coord, data] : StackSnapshots)
	{
		hillNodes.insert(coord);

		if (data.Depth == 0 && inHill)
		{
			// hill -> ground
			if (goodHill && highestPeak <= 11 && !data.InterruptHill && data.Depth <= 11 && false) // TODO: registers are broken
			{
				// hill seems safe enough, use registers to optimize program
				for (auto node : hillNodes)
					StackSnapshots[node].CanUseRegisters = true;
			}
			else
			{
				// only update highest peak count when we aren't using registers
				if (highestPeak > HighestPeak)
					HighestPeak = highestPeak;
			}

			hillNodes.clear();
			inHill = false;
			goodHill = true;
			highestPeak = 0;
		}
		else if (data.Depth != 0 && !inHill)
		{
			// ground -> hill
			inHill = true;
		}

		if (data.InterruptHill)
			goodHill = false;

		if (data.Depth > highestPeak)
			highestPeak = data.Depth;
	}
}

int JIT::Function::GetStackPeak()
{
	if (CallIndirectUsed)
		return HighestPeak + (10 * NumCallIndirects); // TODO: 10 is a random guess
	else
		return HighestPeak;
}

// Sample stack view
// S0 S1 S2 S3 L1 L2 L3 L4 R0 R1 R2 |RA| P0 P1 P2 P3 |S0 S1 S2 S3|

asmjit::Operand JIT::Function::GetParameter(int index, int size)
{
	int paramStartLocation = (GetStackPeak() * 8) + (NumLocals * 8) + (NumReturns * 8) + 8;

	if (File->CallIndirectUsed)
		paramStartLocation += 8;

	if (CallIndirectUsed)
		return ptr(GetStableStackReg(), paramStartLocation + (index * 8), size);
	else
		return ptr(rsp, paramStartLocation + (index * 8), size);
}

asmjit::Operand JIT::Function::GetLocalFrame(int index, int size)
{
	int localStartLocation = (GetStackPeak() * 8);

	if (CallIndirectUsed)
		return ptr(GetStableStackReg(), localStartLocation + (index * 8), size);
	else
		return ptr(rsp, localStartLocation + (index * 8), size);
}

asmjit::Operand JIT::Function::GetLocal(int index, int size)
{
	if (index < NumParams)
		return GetParameter(index, size);
	else if (index >= NumParams + 2)
		return GetLocalFrame(index - 2 - NumParams, size);

	throw std::runtime_error("Invalid local index"); // should never reach here
}

asmjit::Operand JIT::Function::GetReturn(int index, int size)
{
	int returnStartLocation = (GetStackPeak() * 8) + (NumLocals * 8) - 8; // uhhh

	if (CallIndirectUsed)
		return ptr(GetStableStackReg(), returnStartLocation + (index * 8), size);
	else
		return ptr(rsp, returnStartLocation + (index * 8), size);
}

void JIT::Function::SeekIP(int value)
{
	CurrentInstructionPointer += value;
}

int JIT::Function::GetRelIP()
{
	return CurrentInstructionPointer;
}

int JIT::Function::GetFileIP()
{
	return CodeStartOffset + CurrentInstructionPointer;
}

void JIT::Function::CheckLabels(Builder* a)
{
	auto label_copy = std::vector<asmjit::LabelNode*>();
	for (auto& [_, l] : Labels)
		label_copy.push_back(a->_labelNodes[l.id()]);

	for (auto node = FirstNode; node; node = node->next())
	{
		if (node->isLabel())
			std::erase(label_copy, node);
	}

	if (label_copy.size() != 0)
		throw std::runtime_error("Label not bound");
}

bool JIT::Function::UsingRegisters()
{
	return StackSnapshots[CurrentInstructionPointer].CanUseRegisters;
}

asmjit::Operand JIT::Function::GetStackItem(int index, int size)
{
	if (UsingRegisters())
		return GetStackRegister(index, size);

	int stackStartLocation = 0;
	auto operand           = ptr(rsp, (int)stackStartLocation + (int)(index * 8), size);

	return operand;
}

asmjit::Operand JIT::Function::PushToStack(int size)
{
	CurrentStackDepth += 1;
	return GetStackItem(CurrentStackDepth - 1, size);
}

asmjit::Operand JIT::Function::PopFromStack(int size)
{
	CurrentStackDepth -= 1;
	return GetStackItem(CurrentStackDepth, size);
}

asmjit::Operand JIT::Function::GetStackTop(int size)
{
	return GetStackItem(CurrentStackDepth - 1, size);
}

JIT::Function::Function(ScriptFile* file, int begin, int end, int params, int locals, int returns) :
	File(file),
	CodeStartOffset(begin),
	CodeEndOffset(end)
{
	static int counter = 0;
	NumParams  = params;
	NumLocals  = locals;
	NumReturns = returns;
	Id         = counter++;

	ASMJIT_ASSERT(params >= 0 && params <= 2000);
	ASMJIT_ASSERT(locals >= 0 && locals <= 5000);
	ASMJIT_ASSERT(returns >= 0 && returns <= 2000);

#if 0
	LOG(VERBOSE) << __FUNCTION__ << ": [" << CodeStartOffset << ", " << CodeEndOffset << "]: params=" << NumParams << " locals=" << NumLocals << " returns=" << NumReturns;
#endif
}

void JIT::Function::Preprocess(Builder* a)
{
	FunctionLabel = a->newNamedLabel(("FUNCTION_" + std::to_string(Id)).data());
	CurrentInstructionPointer = 0;
	int stackOffset      = 0;
	int oldStackOffset        = 0;
	bool constantPushed       = false;

	while (GetFileIP() < CodeEndOffset)
	{
		Opcode opcode = File->GetOpcode(GetFileIP());

		// stack processing
		StackSnapshot snapshot{};

		// labels
		if (IsJumpInstruction(opcode))
		{
			if (File->GetInsnRelJmpAddr(GetFileIP()) <= CodeStartOffset || File->GetInsnRelJmpAddr(GetFileIP()) >= CodeEndOffset || File->GetInsnRelJmpAddr(GetFileIP()) == GetFileIP() + 3)
			{
				snapshot.Skip = true; // skip broken jumps that Rockstar's compiler generates for no reason
			}
			else
			{
				if (!Labels.contains(File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset))
					Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset] = a->newLabel();
			}
		}

		if (opcode >= Opcode::PUSH_CONST_M1 && opcode <= Opcode::PUSH_CONST_7)
		{
			FakeStack.push_back({((int)opcode) - (int)Opcode::PUSH_CONST_0, GetRelIP(), true, -1});
			stackOffset++;
			constantPushed = true;
		}
		else switch (opcode)
		{
		case Opcode::PUSH_CONST_U8:
		case Opcode::PUSH_CONST_S16:
		case Opcode::PUSH_CONST_U24:
		case Opcode::PUSH_CONST_U32: 
		{
			FakeStack.push_back({
				(int)File->GetInsnOperand(GetFileIP(), 0), GetRelIP(), true, -1
			});
			stackOffset++;
			constantPushed = true;
			break;
		}
		case Opcode::PUSH_CONST_U8_U8:
		{
			FakeStack.push_back({(int)File->GetInsnOperand(GetFileIP(), 0), GetRelIP(), true, 0});
			FakeStack.push_back({(int)File->GetInsnOperand(GetFileIP(), 1), GetRelIP(), true, 1});
			stackOffset += 2;
			constantPushed = true;
			break;
		}
		case Opcode::PUSH_CONST_U8_U8_U8:
		{
			FakeStack.push_back({(int)File->GetInsnOperand(GetFileIP(), 0), GetRelIP(), true, 0});
			FakeStack.push_back({(int)File->GetInsnOperand(GetFileIP(), 1), GetRelIP(), true, 1});
			FakeStack.push_back({(int)File->GetInsnOperand(GetFileIP(), 1), GetRelIP(), true, 2});
			stackOffset += 3;
			constantPushed = true;
			break;
		}
		case Opcode::TEXT_LABEL_ASSIGN_STRING:
		case Opcode::TEXT_LABEL_ASSIGN_INT:
		case Opcode::TEXT_LABEL_APPEND_STRING:
		case Opcode::TEXT_LABEL_APPEND_INT:
		case Opcode::F2I:
		case Opcode::IDIV:
		case Opcode::IMOD:
		{
			snapshot.InterruptHill = true;
			goto next;
		}
		case Opcode::NATIVE: 
		{
			snapshot.InterruptHill = true;

			uint16_t i0            = File->GetInsnOperand(GetFileIP(), 1);
			uint8_t i1             = File->GetInsnOperand(GetFileIP(), 0);
			uint8_t i2             = File->GetInsnOperand(GetFileIP(), 2);
			int num_params         = (i1 >> 2) & 0x3F;
			int num_returns        = i1 & 3;
			auto entrypoint        = File->Program->m_native_entrypoints[(uint64_t)(i0 << 8) | (uint64_t)i2];

			if (entrypoint == NETWORK_SET_RICH_PRESENCE)
				Broken = true;

			stackOffset -= (num_params - num_returns);

			break;
		}
		case Opcode::LEAVE: 
		{
			stackOffset -= NumReturns;
			break;
		}
		case Opcode::LOAD_N:
		{
			stackOffset -= (2 - FakeStack.at(FakeStack.size() - 2).Constant);
			snapshot.ConstantArgData.ConstantArg1 = FakeStack.at(FakeStack.size() - 2).Constant;
			MarkPushHandled(FakeStack.at(FakeStack.size() - 2));
			break;
		}
		case Opcode::STORE_N: 
		{
			stackOffset -= (2 + FakeStack.at(FakeStack.size() - 2).Constant);
			snapshot.ConstantArgData.ConstantArg1 = FakeStack.at(FakeStack.size() - 2).Constant;
			MarkPushHandled(FakeStack.at(FakeStack.size() - 2));
			break;
		}
		case Opcode::CALL: 
		{
			snapshot.InterruptHill = true;
			auto function          = File->GetFunctionAtAddress(File->GetInsnOperand(GetFileIP(), 0));
			stackOffset -= (function->NumParams - function->NumReturns);
			function->Xrefs++;
			break;
		}
		case Opcode::TEXT_LABEL_COPY: 
		{
			int unk = FakeStack.at(FakeStack.size() - 2).Constant;
			int num = FakeStack.at(FakeStack.size() - 3).Constant;

			stackOffset -= 3;
			if (num > unk)
			{
				stackOffset -= (num - unk);
				num -= (num - unk);
			}
			if (num > 0)
				stackOffset -= num;

			MarkPushHandled(FakeStack.at(FakeStack.size() - 2));
			MarkPushHandled(FakeStack.at(FakeStack.size() - 3));

			snapshot.ConstantArgData.ConstantArg1 = FakeStack.at(FakeStack.size() - 2).Constant;
			snapshot.ConstantArgData.ConstantArg2 = FakeStack.at(FakeStack.size() - 3).Constant;

			break;
		}
		case Opcode::STRING:
		{
			snapshot.ConstantArgData.ConstantArg1 = FakeStack.at(FakeStack.size() - 1).Constant;
			MarkPushHandled(FakeStack.at(FakeStack.size() - 1));
			break;
		}
		case Opcode::SWITCH:
		{ 
			uint32_t count = *File->Program->get_code_address(GetFileIP() + 1);
			for (int i = 0; i < count; i++)
				if (!Labels.contains((*(uint16_t*)File->Program->get_code_address(GetFileIP() + 2 + (i * 6) + 4) + (i + 1) * 6) + GetRelIP() + 2))
					Labels[(*(uint16_t*)File->Program->get_code_address(GetFileIP() + 2 + (i * 6) + 4) + (i + 1) * 6) + GetRelIP() + 2] = a->newLabel();
			goto next;
		}
		case Opcode::CALLINDIRECT:
		{
			CallIndirectUsed          = true;
			NumCallIndirects++;
			File->CallIndirectUsed = true;
			break;
		}
		default: 
		{
			next:
			stackOffset += g_OpcodeArgs[(int)opcode].second.second;
			break;
		}
		}

		snapshot.Depth = stackOffset;
		snapshot.Delta             = stackOffset - oldStackOffset;
		StackSnapshots[GetRelIP()] = snapshot;

		SeekIP(File->GetInsnSize(GetFileIP()));

		if (oldStackOffset != stackOffset)
		{
			if (stackOffset < 0)
				stackOffset = 0; // CALLINDIRECT

			if (!constantPushed)
			{
				if (oldStackOffset < stackOffset)
					for (int i = oldStackOffset; i < stackOffset; i++)
						FakeStack.push_back({});
				else
					for (int i = stackOffset; i < oldStackOffset; i++)
						FakeStack.pop_back();
			}
			oldStackOffset = stackOffset;
		}
		constantPushed = false;
	}

	if (stackOffset != 0 && !CallIndirectUsed && !Broken)
		throw std::runtime_error("Stack isn't balanced");

	if (!Broken)
		ProcessStackHills();
}

void JIT::Function::Assemble(Builder* a)
{
	a->bind(FunctionLabel);
	FirstNode = a->cursor(); // actually the last node of the previous function
	a->commentf("NumParams: %d", NumParams);
	a->commentf("StackSize: %d", GetStackPeak() * 8);
	a->commentf("LocalSize: %d", NumLocals * 8);
	a->commentf("ReturnSize: %d", NumReturns * 8);

	if (Broken)
	{
		a->comment("Broken function, skipping");
		a->ret(); // broken functions shouldn't return anything
		return;
	}

	CurrentInstructionPointer = 0;

	// create storage space for locals
	if (NumLocals + GetStackPeak() + NumReturns)
		a->sub(rsp, (NumLocals + GetStackPeak() + NumReturns) * 8);

	if (CallIndirectUsed)
		Move(*a, GetStableStackReg(), rsp);

	// TODO: this is very inefficient
	for (int i = 0; i < NumLocals; i++)
		Move(*a, GetLocalFrame(i), asmjit::Imm(0));

	while (GetFileIP() < CodeEndOffset)
	{
		if (Labels.contains(GetRelIP()))
			a->bind(Labels.at(GetRelIP()));

		if (StackSnapshots[GetRelIP()].Skip)
		{
			if (IsJumpInstruction(File->GetOpcode(GetFileIP())))
				CurrentStackDepth += StackSnapshots[GetRelIP()].Delta;
			SeekIP(File->GetInsnSize(GetFileIP()));
			continue;
		}

		auto opcode = File->GetOpcode(GetFileIP());

		switch (opcode)
		{
		case Opcode::ENTER:
		{
			break;
		}
		case Opcode::LEAVE:
		{
			for (int i = NumReturns; i > 0; i--)
				Move(*a, GetReturn(i), PopFromStack());

			if (CallIndirectUsed)
				a->mov(rsp, GetStableStackReg()); // stabilize stack

			// fix our frame when we leave
			if (NumLocals + GetStackPeak() + NumReturns)
				a->add(rsp, (NumLocals + GetStackPeak() + NumReturns) * 8);

			if (CodeStartOffset == 0) // main function
			{
				a->call(Context::GetThreadKill()); // kill thread on return from main
			}
			else
			{
				a->ret(); // return to caller
			}
			break;
		}
		case Opcode::CALL:
		{
			auto function = File->GetFunctionAtAddress(File->GetInsnOperand(GetFileIP(), 0));

			if (function->NumParams * 8)
				a->sub(rsp, function->NumParams * 8);

			CurrentStackDepth += function->NumParams;

			for (int i = 0; i < function->NumParams; i++)
				Move(*a, ptr(rsp, (function->NumParams * 8) - ((i + 1) * 8)), PopFromStack()); // TODO: i + 1?

			if (File->CallIndirectUsed)
				a->push(GetStableStackReg());
			a->call(function->FunctionLabel);
			if (File->CallIndirectUsed)
				a->pop(GetStableStackReg());

			if (function->NumParams * 8)
				a->add(rsp, function->NumParams * 8);

			CurrentStackDepth -= function->NumParams;

			int returnOffset = 0 - (function->NumParams * 8) - ((int)File->CallIndirectUsed * 8) - 8 - (int)((int)function->NumReturns * 8);

			for (int i = 0; i < function->NumReturns; i++)
				Move(*a, PushToStack(), ptr(rsp, returnOffset + (i * 8)));

			break;
		}
		case Opcode::PUSH_CONST_U8:
		case Opcode::PUSH_CONST_U24:
		case Opcode::PUSH_CONST_U32:
		case Opcode::PUSH_CONST_F:
		{
			if (!StackSnapshots[GetRelIP()].SuppressPushes[0])
				Move(*a, PushToStack(4), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			break;
		}
		case Opcode::PUSH_CONST_S16:
		{
			if (!StackSnapshots[GetRelIP()].SuppressPushes[0])
				Move(*a, PushToStack(4), asmjit::Imm(File->GetInsnOperandSigned(GetFileIP(), 0)));
			break;
		}
		case Opcode::PUSH_CONST_U8_U8:
		{
			if (!StackSnapshots[GetRelIP()].SuppressPushes[0])
				Move(*a, PushToStack(4), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			if (!StackSnapshots[GetRelIP()].SuppressPushes[1])
				Move(*a, PushToStack(4), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 1)));
			break;
		}
		case Opcode::PUSH_CONST_U8_U8_U8:
		{
			if (!StackSnapshots[GetRelIP()].SuppressPushes[0])
				Move(*a, PushToStack(4), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			if (!StackSnapshots[GetRelIP()].SuppressPushes[1])
				Move(*a, PushToStack(4), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 1)));
			if (!StackSnapshots[GetRelIP()].SuppressPushes[2])
				Move(*a, PushToStack(4), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 2)));
			break;
		}
		case Opcode::PUSH_CONST_M1:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0xFFFFFFFF));
			break;
		}
		case Opcode::PUSH_CONST_0:
		{
			Move(*a, PushToStack(), asmjit::Imm(0));
			break;
		}
		case Opcode::PUSH_CONST_1:
		{
			Move(*a, PushToStack(4), asmjit::Imm(1));
			break;
		}
		case Opcode::PUSH_CONST_2:
		{
			Move(*a, PushToStack(4), asmjit::Imm(2));
			break;
		}
		case Opcode::PUSH_CONST_3:
		{
			Move(*a, PushToStack(4), asmjit::Imm(3));
			break;
		}
		case Opcode::PUSH_CONST_4:
		{
			Move(*a, PushToStack(4), asmjit::Imm(4));
			break;
		}
		case Opcode::PUSH_CONST_5:
		{
			Move(*a, PushToStack(4), asmjit::Imm(5));
			break;
		}
		case Opcode::PUSH_CONST_6:
		{
			Move(*a, PushToStack(4), asmjit::Imm(6));
			break;
		}
		case Opcode::PUSH_CONST_7:
		{
			Move(*a, PushToStack(4), asmjit::Imm(7));
			break;
		}
		case Opcode::PUSH_CONST_FM1:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0xBF800000));
			break;
		}
		case Opcode::PUSH_CONST_F0:
		{
			Move(*a, PushToStack(), asmjit::Imm(0));
			break;
		}
		case Opcode::PUSH_CONST_F1:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0x3F800000));
			break;
		}
		case Opcode::PUSH_CONST_F2:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0x40000000));
			break;
		}
		case Opcode::PUSH_CONST_F3:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0x40400000));
			break;
		}
		case Opcode::PUSH_CONST_F4:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0x40800000));
			break;
		}
		case Opcode::PUSH_CONST_F5:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0x40A00000));
			break;
		}
		case Opcode::PUSH_CONST_F6:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0x40C00000));
			break;
		}
		case Opcode::PUSH_CONST_F7:
		{
			Move(*a, PushToStack(4), asmjit::Imm(0x40E00000));
			break;
		}
		case Opcode::DROP:
		{
			PopFromStack();
			break;
		}
		case Opcode::LOCAL_U8:
		case Opcode::LOCAL_U16:
		case Opcode::LOCAL_U24:
		{
			auto operand = PushToStack();

			if (operand.isMem())
			{
				a->lea(GetTempReg(), GetLocal(File->GetInsnOperand(GetFileIP(), 0)).as<Mem>());
				Move(*a, operand, GetTempReg());
			}
			else
			{
				a->lea(operand.as<Gp>(), GetLocal(File->GetInsnOperand(GetFileIP(), 0)).as<Mem>());
			}

			break;
		}
		case Opcode::LOCAL_U8_LOAD:
		case Opcode::LOCAL_U16_LOAD:
		case Opcode::LOCAL_U24_LOAD:
		{
			Move(*a, PushToStack(), GetLocal(File->GetInsnOperand(GetFileIP(), 0)).as<Mem>());
			break;
		}
		case Opcode::LOCAL_U8_STORE:
		case Opcode::LOCAL_U16_STORE:
		case Opcode::LOCAL_U24_STORE:
		{
			Move(*a, GetLocal(File->GetInsnOperand(GetFileIP(), 0)).as<Mem>(), PopFromStack());
			break;
		}
		case Opcode::STATIC_U8:
		case Opcode::STATIC_U16:
		{
			a->mov(GetTempReg(), ptr(JIT::GetContextReg(), offsetof(Context, Context::Statics)));
			a->add(GetTempReg(), File->GetInsnOperand(GetFileIP(), 0) * 8);
			Move(*a, PushToStack(), GetTempReg());
			break;
		}
		case Opcode::STATIC_U8_LOAD:
		case Opcode::STATIC_U16_LOAD:
		{
			a->mov(GetTempReg2(), ptr(JIT::GetContextReg(), offsetof(Context, Context::Statics)));
			Move(*a, PushToStack(), ptr(GetTempReg2(), File->GetInsnOperand(GetFileIP(), 0) * 8));
			break;
		}
		case Opcode::STATIC_U8_STORE:
		case Opcode::STATIC_U16_STORE:
		{
			a->mov(GetTempReg2(), ptr(JIT::GetContextReg(), offsetof(Context, Context::Statics)));
			Move(*a, ptr(GetTempReg2(), File->GetInsnOperand(GetFileIP(), 0) * 8), PopFromStack());
			break;
		}
		case Opcode::IOFFSET:
		{
			auto sval = PopFromStack(4);
			if (sval.isMem())
				a->movsxd(GetTempReg(), sval.as<Mem>());
			else
				a->movsxd(GetTempReg(), sval.as<Gp>());
			a->shl(GetTempReg(), 3);
			Add(*a, GetStackTop(), GetTempReg());
			break;
		}
		case Opcode::IOFFSET_U8:
		{
			Add(*a, GetStackTop(), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0) * 8));
			break;
		}
		case Opcode::IOFFSET_U8_LOAD:
		{
			Move(*a, GetTempReg2(), GetStackTop());
			Move(*a, GetStackTop(), ptr(GetTempReg2(), File->GetInsnOperand(GetFileIP(), 0) * 8));
			break;
		}
		case Opcode::IOFFSET_U8_STORE:
		{
			Move(*a, GetTempReg2(), PopFromStack());
			Move(*a, ptr(GetTempReg2(), File->GetInsnOperand(GetFileIP(), 0) * 8), PopFromStack());
			break;
		}
		case Opcode::IOFFSET_S16:
		{
			int32_t value = File->GetInsnOperandSigned(GetFileIP(), 0) * 8;
			Add(*a, GetStackTop(), asmjit::Imm(value));
			break;
		}
		case Opcode::IOFFSET_S16_LOAD:
		{
			int32_t value = File->GetInsnOperandSigned(GetFileIP(), 0) * 8;
			Move(*a, GetTempReg2(), GetStackTop());
			Move(*a, GetStackTop(), ptr(GetTempReg2(), value));
			break;
		}
		case Opcode::IOFFSET_S16_STORE:
		{
			int32_t value = File->GetInsnOperandSigned(GetFileIP(), 0) * 8;
			Move(*a, GetTempReg2(), PopFromStack());
			Move(*a, ptr(GetTempReg2(), value), PopFromStack());
			break;
		}
		case Opcode::GLOBAL_U16:
		case Opcode::GLOBAL_U24:
		{
			int index = File->GetInsnOperand(GetFileIP(), 0);
			Move(*a, GetTempReg(), ptr(JIT::GetContextReg(), offsetof(Context, Context::Globals)));
			Move(*a, GetTempReg(), ptr(GetTempReg(), (index >> 18) * 8, 8));
			Add(*a, GetTempReg(), asmjit::Imm((index & 0x3FFFF) * 8));
			Move(*a, PushToStack(), GetTempReg());
			break;
		}
		case Opcode::GLOBAL_U16_LOAD:
		case Opcode::GLOBAL_U24_LOAD:
		{
			int index = File->GetInsnOperand(GetFileIP(), 0);
			Move(*a, GetTempReg2(), ptr(JIT::GetContextReg(), offsetof(Context, Context::Globals)));
			Move(*a, GetTempReg2(), ptr(GetTempReg2(), (index >> 18) * 8, 8));
			Move(*a, PushToStack(), ptr(GetTempReg2(), (index & 0x3FFFF) * 8, 8));
			break;
		}
		case Opcode::GLOBAL_U16_STORE:
		case Opcode::GLOBAL_U24_STORE:
		{
			int index = File->GetInsnOperand(GetFileIP(), 0);
			Move(*a, GetTempReg2(), ptr(JIT::GetContextReg(), offsetof(Context, Context::Globals)));
			Move(*a, GetTempReg2(), ptr(GetTempReg2(), (index >> 18) * 8, 8));
			Move(*a, ptr(GetTempReg2(), (index & 0x3FFFF) * 8, 8), PopFromStack());
			break;
		}
		case Opcode::LOAD:
		{
			if (GetStackTop().isReg())
				Move(*a, GetStackTop(), ptr(GetStackTop().as<Gp>()));
			else
			{
				Move(*a, GetTempReg(), GetStackTop());
				Move(*a, GetStackTop(), ptr(GetTempReg()));
			}
			break;
		}
		case Opcode::STORE:
		{
			auto target = PopFromStack();
			auto item   = PopFromStack();
			
			Move(*a, GetTempReg(), target);
			Move(*a, GetTempReg2(), item);
			Move(*a, ptr(GetTempReg()), GetTempReg2());
			break;
		}
		case Opcode::STORE_REV:
		{
			auto item = PopFromStack();
			auto target = GetStackTop();

			Move(*a, GetTempReg(), target);
			Move(*a, GetTempReg2(), item);
			Move(*a, ptr(GetTempReg()), GetTempReg2());
			break;
		}
		case Opcode::STRING:
		{
			if (!StackSnapshots[GetRelIP()].ConstantArgData.ConstantArg1.has_value())
				throw std::runtime_error("Invalid string table access");

			Move(*a,
				GetTempReg(),
				asmjit::Imm((uint64_t)File->Program->get_string(StackSnapshots[GetRelIP()].ConstantArgData.ConstantArg1.value()))); // cannot move 64 bit values to memory directly
			Move(*a,
				PushToStack(),
				GetTempReg());
			break;
		}
		case Opcode::IS_BIT_SET:
		{
			Move(*a, GetTempReg(4), PopFromStack()); // a2
			Move(*a, GetTempReg2(4), PopFromStack()); // a1
			a->bt(GetTempReg2(4), GetTempReg(4));
			a->setb(GetTempReg(1));
			a->movzx(GetTempReg(4), GetTempReg(1)); // clear top
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::J:
		{
			a->jmp(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::JZ:
		{
			Compare(*a, PopFromStack(4), asmjit::Imm(0));
			a->jz(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::IEQ_JZ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->jnz(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::INE_JZ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->jz(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::IGT_JZ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->jle(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::IGE_JZ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->jl(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::ILT_JZ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->jge(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::ILE_JZ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->jg(Labels[File->GetInsnRelJmpAddr(GetFileIP()) - CodeStartOffset]);
			break;
		}
		case Opcode::IADD:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop();
			Add(*a, v0, v1);
			break;
		}
		case Opcode::ISUB:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop();
			Sub(*a, v0, v1);
			break;
		}
		case Opcode::IMUL:
		{
			auto v1 = PopFromStack();
			auto v0 = GetStackTop(4);
			IMul(*a, v0, v1);
			break;
		}
		case Opcode::IDIV:
		{
			auto v1   = GetStackTop(4);
			auto skip = a->newLabel();
			Compare(*a, v1, asmjit::Imm(0));
			a->jz(skip); // skip division if zero

			a->push(rax); CurrentStackDepth += 1; // store our context

			auto v0 = GetStackItem(CurrentStackDepth - 2, 4);
			v1      = GetStackTop(4); // refresh stack var

			a->xor_(edx, edx);
			Move(*a, eax, v0); // EDX:EAX - dividend
			a->cdq(); // not sure what this does but the game does it so...

			if (v1.isReg())
				a->idiv(v1.as<Gp>());
			else
				a->idiv(v1.as<Mem>());

			// EAX - quotient, EDX - remainder

			Move(*a, v0, eax);

			a->pop(rax); CurrentStackDepth -= 1; // restore
			(void)PopFromStack();
			a->bind(skip);
			break;
		}
		case Opcode::IMOD:
		{
			auto v1   = GetStackTop(4);
			auto skip = a->newLabel();
			Compare(*a, v1, asmjit::Imm(0));
			a->jz(skip); // skip division if zero

			a->push(rax);
			CurrentStackDepth += 1; // store our context

			auto v0 = GetStackItem(CurrentStackDepth - 2, 4);
			v1      = GetStackTop(4); // refresh stack var

			a->xor_(edx, edx);
			Move(*a, eax, v0); // EDX:EAX - dividend
			a->cdq();          // not sure what this does but the game does it so...

			if (v1.isReg())
				a->idiv(v1.as<Gp>());
			else
				a->idiv(v1.as<Mem>());

			// EAX - quotient, EDX - remainder

			Move(*a, v0, edx);

			a->pop(rax);
			CurrentStackDepth -= 1; // restore
			(void)PopFromStack();
			a->bind(skip);
			break;
		}
		case Opcode::INOT:
		{
			auto v0 = GetStackTop(4);
			a->xor_(GetTempReg2(), GetTempReg2());
			Compare(*a, v0, asmjit::Imm(0), 4);
			a->setz(GetTempReg2(1));
			Move(*a, v0, GetTempReg2(4));
			break;
		}
		case Opcode::INEG:
		{
			auto v0 = GetStackTop(4);
			if (v0.isReg())
				a->neg(v0.as<Gp>());
			else
				a->neg(v0.as<Mem>());
			break;
		}
		case Opcode::IEQ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->setz(GetTempReg(1));
			a->movzx(GetTempReg(4), GetTempReg(1)); // clear top
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::INE:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->setnz(GetTempReg(1));
			a->movzx(GetTempReg(4), GetTempReg(1)); // clear top
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::IGT:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->setg(GetTempReg(1));
			a->movzx(GetTempReg(4), GetTempReg(1)); // clear top
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::IGE:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->setge(GetTempReg(1));
			a->movzx(GetTempReg(4), GetTempReg(1)); // clear top
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::ILT:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->setl(GetTempReg(1));
			a->movzx(GetTempReg(4), GetTempReg(1)); // clear top
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::ILE:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Compare(*a, v0, v1, 4);
			a->movzx(GetTempReg(4), GetTempReg(1)); // clear top
			a->setle(GetTempReg(1));
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::IAND:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop();
			And(*a, v0, v1);
			break;
		}
		case Opcode::IOR:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop();
			Or(*a, v0, v1);
			break;
		}
		case Opcode::IXOR:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop();
			Xor(*a, v0, v1);
			break;
		}
		case Opcode::F2I:
		{
			auto v0 = PopFromStack(4);
			if (v0.isReg())
				throw std::runtime_error("Unimplemented"); // TODO
			else
				a->cvttss2si(GetTempReg(4), v0.as<Mem>());
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::I2F:
		{
			auto v0 = GetStackTop(4);
			Load(*a, xmm0, v0);
			a->cvtdq2ps(xmm0, xmm0);
			Store(*a, v0, xmm0);
			break;
		}
		case Opcode::F2V:
		{
			auto v0 = GetStackTop(4);
			Move(*a, PushToStack(), v0);
			Move(*a, PushToStack(), v0);
			break;
		}
		case Opcode::DUP:
		{
			auto v0 = GetStackTop();
			Move(*a, PushToStack(), v0);
			break;
		}
		case Opcode::FADD:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->addss(xmm0, xmm1);
			Store(*a, v0, xmm0);
			break;
		}
		case Opcode::FSUB:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->subss(xmm0, xmm1);
			Store(*a, v0, xmm0);
			break;
		}
		case Opcode::FMUL:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->mulss(xmm0, xmm1);
			Store(*a, v0, xmm0);
			break;
		}
		case Opcode::FDIV:
		{
			auto v1 = PopFromStack(4);
			auto v0 = GetStackTop(4);
			auto skip = a->newLabel();
			Compare(*a, v1, asmjit::Imm(0));
			a->jz(skip);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->divss(xmm0, xmm1);
			Store(*a, v0, xmm0);
			a->bind(skip);
			break;
		}
		case Opcode::FMOD:
		{
			auto v1   = PopFromStack(4);
			auto v0   = GetStackTop(4);
			auto skip = a->newLabel();
			Compare(*a, v1, asmjit::Imm(0));
			a->jz(skip);
			Load(*a, xmm3, v1);
			Load(*a, xmm2, v0);
			a->movaps(xmm0, xmm2);
			a->divss(xmm0, xmm3);
			a->cvttss2si(GetTempReg(4), xmm0);
			a->movd(xmm1, GetTempReg(4));
			a->cvtdq2ps(xmm1, xmm1);
			a->mulss(xmm1, xmm3);
			a->subss(xmm2, xmm1);
			Store(*a, v0, xmm2);
			a->bind(skip);
			break;
		}
		case Opcode::FNEG:
		{
			auto v0 = GetStackTop(4);
			if (v0.isReg())
				a->btc(v0.as<Gp>(), 0x1F);
			else
				a->btc(v0.as<Mem>(), 0x1F);
			break;
		}
		case Opcode::FEQ:
		{
			auto v1 = PopFromStack(4);
			auto v0 = PopFromStack(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->xor_(GetTempReg(), GetTempReg());
			a->ucomiss(xmm0, xmm1);
			a->setz(GetTempReg(1));
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::FNE:
		{
			auto v0 = PopFromStack(4);
			auto v1 = PopFromStack(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->xor_(GetTempReg(), GetTempReg());
			a->ucomiss(xmm0, xmm1);
			a->setnz(GetTempReg(1));
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::FGT:
		{
			auto v0 = PopFromStack(4);
			auto v1 = PopFromStack(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->xor_(GetTempReg(), GetTempReg());
			a->comiss(xmm0, xmm1);
			a->setb(GetTempReg(1));
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::FGE:
		{
			auto v0 = PopFromStack(4);
			auto v1 = PopFromStack(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->xor_(GetTempReg(), GetTempReg());
			a->comiss(xmm0, xmm1);
			a->setbe(GetTempReg(1));
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::FLT:
		{
			auto v0 = PopFromStack(4);
			auto v1 = PopFromStack(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->xor_(GetTempReg(), GetTempReg());
			a->comiss(xmm1, xmm0); // registers swapped for this one
			a->setb(GetTempReg(1));
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::FLE:
		{
			auto v0 = PopFromStack(4);
			auto v1 = PopFromStack(4);
			Load(*a, xmm0, v0);
			Load(*a, xmm1, v1);
			a->xor_(GetTempReg(), GetTempReg());
			a->comiss(xmm1, xmm0); // registers swapped for this one
			a->setbe(GetTempReg(1));
			Move(*a, PushToStack(), GetTempReg(4));
			break;
		}
		case Opcode::VADD:
		{
			auto z2 = PopFromStack(4);
			auto y2 = PopFromStack(4);
			auto x2 = PopFromStack(4);
			auto z1 = GetStackItem(CurrentStackDepth - 1, 4);
			auto y1 = GetStackItem(CurrentStackDepth - 2, 4);
			auto x1 = GetStackItem(CurrentStackDepth - 3, 4);

			Load(*a, xmm0, x1);
			Load(*a, xmm1, y1);
			Load(*a, xmm2, z1);
			Load(*a, xmm3, x2);
			Load(*a, xmm4, y2);
			Load(*a, xmm5, z2);

			a->addss(xmm0, xmm3);
			a->addss(xmm1, xmm4);
			a->addss(xmm2, xmm5);

			Store(*a, x1, xmm0);
			Store(*a, y1, xmm1);
			Store(*a, z1, xmm2);

			break;
		}
		case Opcode::VSUB:
		{
			auto z2 = PopFromStack(4);
			auto y2 = PopFromStack(4);
			auto x2 = PopFromStack(4);
			auto z1 = GetStackItem(CurrentStackDepth - 1, 4);
			auto y1 = GetStackItem(CurrentStackDepth - 2, 4);
			auto x1 = GetStackItem(CurrentStackDepth - 3, 4);

			Load(*a, xmm0, x1);
			Load(*a, xmm1, y1);
			Load(*a, xmm2, z1);
			Load(*a, xmm3, x2);
			Load(*a, xmm4, y2);
			Load(*a, xmm5, z2);

			a->subss(xmm0, xmm3);
			a->subss(xmm1, xmm4);
			a->subss(xmm2, xmm5);

			Store(*a, x1, xmm0);
			Store(*a, y1, xmm1);
			Store(*a, z1, xmm2);

			break;
		}
		case Opcode::VMUL:
		{
			auto z2 = PopFromStack(4);
			auto y2 = PopFromStack(4);
			auto x2 = PopFromStack(4);
			auto z1 = GetStackItem(CurrentStackDepth - 1, 4);
			auto y1 = GetStackItem(CurrentStackDepth - 2, 4);
			auto x1 = GetStackItem(CurrentStackDepth - 3, 4);

			Load(*a, xmm0, x1);
			Load(*a, xmm1, y1);
			Load(*a, xmm2, z1);
			Load(*a, xmm3, x2);
			Load(*a, xmm4, y2);
			Load(*a, xmm5, z2);

			a->mulss(xmm0, xmm3);
			a->mulss(xmm1, xmm4);
			a->mulss(xmm2, xmm5);

			Store(*a, x1, xmm0);
			Store(*a, y1, xmm1);
			Store(*a, z1, xmm2);

			break;
		}
		case Opcode::VDIV:
		{
			auto z2 = PopFromStack(4);
			auto y2 = PopFromStack(4);
			auto x2 = PopFromStack(4);
			auto z1    = GetStackItem(CurrentStackDepth - 1, 4);
			auto y1    = GetStackItem(CurrentStackDepth - 2, 4);
			auto x1    = GetStackItem(CurrentStackDepth - 3, 4);
			auto skip1 = a->newLabel();
			auto skip2 = a->newLabel();
			auto skip3 = a->newLabel();

			Load(*a, xmm0, x1);
			Load(*a, xmm1, y1);
			Load(*a, xmm2, z1);
			Load(*a, xmm3, x2);
			Load(*a, xmm4, y2);
			Load(*a, xmm5, z2);

			Compare(*a, x2, asmjit::Imm(0));
			a->jz(skip1);
			a->divss(xmm0, xmm3);
			a->bind(skip1);
			Compare(*a, y2, asmjit::Imm(0));
			a->jz(skip2);
			a->divss(xmm1, xmm4);
			a->bind(skip2);
			Compare(*a, z2, asmjit::Imm(0));
			a->jz(skip3);
			a->divss(xmm2, xmm5);
			a->bind(skip3);

			Store(*a, x1, xmm0);
			Store(*a, y1, xmm1);
			Store(*a, z1, xmm2);

			break;
		}
		case Opcode::VNEG:
		{
			auto z = GetStackItem(CurrentStackDepth - 1);
			auto y = GetStackItem(CurrentStackDepth - 2);
			auto x = GetStackItem(CurrentStackDepth - 3);
			Move(*a, GetTempReg(4), asmjit::Imm(0x80000000));
			Xor(*a, x, GetTempReg(4));
			Xor(*a, y, GetTempReg(4));
			Xor(*a, z, GetTempReg(4));
			break;
		}
		case Opcode::NATIVE:
		{
			uint16_t i0 = File->GetInsnOperand(GetFileIP(), 1);
			uint8_t i1  = File->GetInsnOperand(GetFileIP(), 0);
			uint8_t i2  = File->GetInsnOperand(GetFileIP(), 2);
			int num_params = (i1 >> 2) & 0x3F;
			int num_returns = i1 & 3;
			auto entrypoint  = File->Program->m_native_entrypoints[(uint64_t)(i0 << 8) | (uint64_t)i2];
			bool mapped_refs = NeedToFixVecRefrs(entrypoint);

			if (entrypoint == TERMINATE_THIS_THREAD)
			{
				a->jmp(Context::GetThreadKill());
				break;
			}
			else if (entrypoint == WAIT)
			{
				Move(*a, ptr(JIT::GetContextReg(), offsetof(Context, Context::SleepMsecs), 4), PopFromStack(4), 4);
				Move(*a, ptr(JIT::GetContextReg(), offsetof(Context, Context::SleepQueued), 1), asmjit::Imm(1), 1);
				a->call(Context::GetVMExit());
				break;
			}
			else if (entrypoint == HAS_FORCE_CLEANUP_OCCURRED)
			{
				Move(*a, ptr(JIT::GetContextReg(), offsetof(Context, Context::ForceCleanupFlags), 4), PopFromStack(4), 4);
				Move(*a, ptr(JIT::GetContextReg(), offsetof(Context, Context::ForceCleanupSetup), 1), asmjit::Imm(1), 1);
				a->call(Context::GetVMExit());
				Xor(*a, GetTempReg2(), GetTempReg2());
				Move(*a, GetTempReg2(1), ptr(JIT::GetContextReg(), offsetof(Context, Context::ForceCleanupActive), 1));
				Move(*a, PushToStack(4), GetTempReg2(4));
				break;
			}

			if (NeedToSetProgramCounter(entrypoint))
			{
				// the PC value doesn't matter as long as it is unique
				Move(*a, GetTempReg2(), ptr(JIT::GetContextReg(), offsetof(Context, Context::Thread)));
				Move(*a, ptr(GetTempReg2(), offsetof(rage::scrThread, rage::scrThread::m_context) + offsetof(rage::scrThreadContext, rage::scrThreadContext::m_instruction_pointer), 4), asmjit::Imm(GetFileIP() + 3));
			}

			if (UsingRegisters())
				throw std::runtime_error("Shouldn't use registers here");

			CurrentStackDepth -= num_params;
			if (mapped_refs)
				Move(*a, ptr(GetContextReg(), offsetof(Context, Context::CallContext) + offsetof(rage::scrNativeCallContext, rage::scrNativeCallContext::m_data_count), 4), asmjit::Imm(0));
			a->lea(GetTempReg2(), GetStackItem(CurrentStackDepth).as<Mem>()); // should now point to the argument list
			Move(*a, ptr(GetContextReg(), offsetof(Context, Context::CallContext) + offsetof(rage::scrNativeCallContext, rage::scrNativeCallContext::m_arg_count), 4), asmjit::Imm(num_params));
			Move(*a, ptr(GetContextReg(), offsetof(Context, Context::CallContext) + offsetof(rage::scrNativeCallContext, rage::scrNativeCallContext::m_args), 8), GetTempReg2());
			if (num_returns)
				Move(*a, ptr(GetContextReg(), offsetof(Context, Context::CallContext) + offsetof(rage::scrNativeCallContext, rage::scrNativeCallContext::m_return_value), 8), GetTempReg2());
			else
				Move(*a, ptr(GetContextReg(), offsetof(Context, Context::CallContext) + offsetof(rage::scrNativeCallContext, rage::scrNativeCallContext::m_return_value), 8), asmjit::Imm(0));

			if (File->CallIndirectUsed)
				a->push(GetStableStackReg());

			a->push(rax); // save the VM context
			a->lea(rcx, ptr(GetContextReg(), offsetof(Context, Context::CallContext)));

			a->mov(rbp, rsp);
			a->and_(rsp, 0xFFFFFFFFFFFFFFF0); // TODO: optimize

			CallDebug(*a, asmjit::Imm(entrypoint));

			a->mov(rsp, rbp);

			a->pop(rax);

			if (mapped_refs)
			{
				a->push(rax);
				a->lea(rcx, ptr(GetContextReg(), offsetof(Context, Context::CallContext)));
				CallDebug(*a, asmjit::Imm(NewBase::Pointers.m_FixVectors)); // don't have to worry about alignment here
				a->pop(rax);
			}

			if (File->CallIndirectUsed)
				a->pop(GetStableStackReg());

			CurrentStackDepth += num_returns;

			break;
		}
		case Opcode::LOAD_N:
		{
			auto pointer = PopFromStack();
			int count    = StackSnapshots[GetRelIP()].ConstantArgData.ConstantArg1.value();

			Move(*a, GetTempReg2(), pointer);
			for (int i = 0; i < count; i++)
				Move(*a, PushToStack(), ptr(GetTempReg2(), i * 8)); // TODO: optimize this

			break;
		}
		case Opcode::STORE_N:
		{
			auto pointer = PopFromStack();
			int count    = StackSnapshots[GetRelIP()].ConstantArgData.ConstantArg1.value();

			Move(*a, GetTempReg2(), pointer);
			for (int i = 0; i < count; i++)
				Move(*a, ptr(GetTempReg2(), ((count - i) - 1) * 8), PopFromStack()); // TODO: optimize this
			break;
		}
		case Opcode::ARRAY_U8:
		case Opcode::ARRAY_U16:
		{
			// TODO: verify this mess

			auto array     = PopFromStack();
			auto index     = PopFromStack();
			auto skip_zero = a->newLabel();

			Move(*a, GetTempReg(), index);
			Move(*a, GetTempReg2(), array);
			Move(*a, GetTempReg2(), ptr(GetTempReg2()));
			Compare(*a, GetTempReg(4), GetTempReg2(4));
			a->jb(skip_zero);
			if (CRASH_ON_OOB_ARRAY_ACCESS)
				a->ud2();
			else
				Move(*a, GetTempReg(), asmjit::Imm(0));
			a->bind(skip_zero);
			Move(*a, GetTempReg2(), array);
			a->mov(GetTempReg(4), GetTempReg(4)); // clear upper 32 bits
			if (File->GetInsnOperand(GetFileIP(), 0) != 1)
				IMul(*a, GetTempReg(), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			a->lea(GetTempReg2(), ptr(GetTempReg2(), GetTempReg(), 3, 8));
			Move(*a, PushToStack(), GetTempReg2());
			break;
		}
		case Opcode::ARRAY_U8_LOAD:
		case Opcode::ARRAY_U16_LOAD:
		{
			// TODO: verify this mess

			auto array     = PopFromStack();
			auto index     = PopFromStack();
			auto skip_zero = a->newLabel();

			Move(*a, GetTempReg(), index);
			Move(*a, GetTempReg2(), array);
			Move(*a, GetTempReg2(), ptr(GetTempReg2()));
			Compare(*a, GetTempReg(4), GetTempReg2(4));
			a->jb(skip_zero);
			if (CRASH_ON_OOB_ARRAY_ACCESS)
				a->ud2();
			else
				Move(*a, GetTempReg(), asmjit::Imm(0));
			a->bind(skip_zero);
			Move(*a, GetTempReg2(), array);
			a->mov(GetTempReg(4), GetTempReg(4)); // clear upper 32 bits
			if (File->GetInsnOperand(GetFileIP(), 0) != 1)
				IMul(*a, GetTempReg(), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			a->lea(GetTempReg2(), ptr(GetTempReg2(), GetTempReg(), 3, 8));
			Move(*a, PushToStack(), ptr(GetTempReg2()));
			break;
		}
		case Opcode::ARRAY_U8_STORE:
		case Opcode::ARRAY_U16_STORE:
		{
			// TODO: verify this mess

			auto array     = PopFromStack();
			auto index     = PopFromStack();
			auto value     = PopFromStack();
			auto skip_zero = a->newLabel();

			Move(*a, GetTempReg(), index);
			Move(*a, GetTempReg2(), array);
			Move(*a, GetTempReg2(), ptr(GetTempReg2()));
			Compare(*a, GetTempReg(4), GetTempReg2(4));
			a->jb(skip_zero);
			if (CRASH_ON_OOB_ARRAY_ACCESS)
				a->ud2();
			else
				Move(*a, GetTempReg(), asmjit::Imm(0));
			a->bind(skip_zero);
			Move(*a, GetTempReg2(), array);
			a->mov(GetTempReg(4), GetTempReg(4)); // clear upper 32 bits
			if (File->GetInsnOperand(GetFileIP(), 0) != 1)
				IMul(*a, GetTempReg(), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			a->lea(GetTempReg2(), ptr(GetTempReg2(), GetTempReg(), 3, 8));
			Move(*a, ptr(GetTempReg2()), value);
			break;
		}
		case Opcode::SWITCH:
		{
			auto value = PopFromStack(4);
			int count = *File->Program->get_code_address(GetFileIP() + 1);
			for (int i = 0; i < count; i++)
			{
				Compare(*a, value, asmjit::Imm(*(uint32_t*)File->Program->get_code_address(GetFileIP() + 2 + (i * 6))), 4);
				a->je(Labels[(*(int16_t*)File->Program->get_code_address(GetFileIP() + 2 + (i * 6) + 4) + (i + 1) * 6) + GetRelIP() + 2]);
			}
			break;
		}
		case Opcode::TEXT_LABEL_ASSIGN_STRING:
		{
			a->push(rax);
			CurrentStackDepth += 1;
			auto dst = PopFromStack();
			auto src = PopFromStack();
			Move(*a, rcx, dst);
			Move(*a, rdx, src);
			Move(*a, r8, asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			CallDebug(*a, asmjit::Imm(TEXT_LABEL_ASSIGN_STRING));
			CurrentStackDepth -= 1;
			a->pop(rax);
			break;
		}
		case Opcode::TEXT_LABEL_ASSIGN_INT:
		{
			a->push(rax);
			CurrentStackDepth += 1;
			auto dst = PopFromStack();
			auto ival = PopFromStack(4);
			Move(*a, rcx, dst);
			Move(*a, edx, ival);
			Move(*a, r8, asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			CallDebug(*a, asmjit::Imm(TEXT_LABEL_ASSIGN_INT));
			CurrentStackDepth -= 1;
			a->pop(rax);
			break;
		}
		case Opcode::TEXT_LABEL_APPEND_STRING:
		{
			a->push(rax);
			CurrentStackDepth += 1;
			auto dst = PopFromStack();
			auto src = PopFromStack();
			Move(*a, rcx, dst);
			Move(*a, rdx, src);
			Move(*a, r8, asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			CallDebug(*a, asmjit::Imm(TEXT_LABEL_APPEND_STRING));
			CurrentStackDepth -= 1;
			a->pop(rax);
			break;
		}
		case Opcode::TEXT_LABEL_APPEND_INT:
		{
			a->push(rax);
			CurrentStackDepth += 1;
			auto dst  = PopFromStack();
			auto ival = PopFromStack(4);
			Move(*a, rcx, dst);
			Move(*a, edx, ival);
			Move(*a, r8, asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			CallDebug(*a, asmjit::Imm(TEXT_LABEL_APPEND_INT));
			CurrentStackDepth -= 1;
			a->pop(rax);
			break;
		}
		case Opcode::TEXT_LABEL_COPY:
		{
			auto pointer = PopFromStack();
			int unk      = StackSnapshots[GetRelIP()].ConstantArgData.ConstantArg1.value();
			int num      = StackSnapshots[GetRelIP()].ConstantArgData.ConstantArg2.value();

			Move(*a, GetTempReg2(), pointer);

			if (num > unk)
			{
				for (int i = 0; i < (num - unk); i++)
					PopFromStack();
				num -= (num - unk);
			}
			if (num > 0)
				for (int i = num; i > 0; i--)
					Move(*a, ptr(GetTempReg2(), (i - 1) * 8), PopFromStack()); // TODO: verify
			Move(*a, ptr(GetTempReg2(), (num - 1) * 8, 8), asmjit::Imm(0));
			break;
		}
		case Opcode::IADD_U8:
		{
			Add(*a, GetStackTop(), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			break;
		}
		case Opcode::IADD_S16:
		{
			Add(*a, GetStackTop(), asmjit::Imm(File->GetInsnOperandSigned(GetFileIP(), 0)));
			break;
		}
		case Opcode::IMUL_U8:
		{
			IMul(*a, GetStackTop(), asmjit::Imm(File->GetInsnOperand(GetFileIP(), 0)));
			break;
		}
		case Opcode::IMUL_S16:
		{
			IMul(*a, GetStackTop(), asmjit::Imm(File->GetInsnOperandSigned(GetFileIP(), 0)));
			break;
		}
		case Opcode::NOP:
		{
			break;
		}
		case Opcode::CALLINDIRECT:
		{
			auto loc = PopFromStack();
			auto skip           = a->newLabel();
			auto paramLoop = a->newLabel();
			auto skipParamLoop = a->newLabel();
			auto returnLoop    = a->newLabel();
			auto skipReturnLoop = a->newLabel();

			// #1: Resolve the function address

			Move(*a, rcx, rax); // context
			Move(*a, rdx, loc); // index

			a->push(rax); // prevent registers from being clobbered
			a->push(r8);

			CurrentStackDepth += 2;

			a->mov(rbp, rsp);
			a->and_(rsp, 0xFFFFFFFFFFFFFFF0); // TODO: remove this after debug
			CallDebug(*a, asmjit::Imm(CALLINDIRECT));
			a->mov(rsp, rbp);

			a->xchg(GetTempReg3(), rax); // store return value

			CurrentStackDepth -= 2;

			a->pop(r8);
			a->pop(rax);

			a->cmp(ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::Function), 8), asmjit::Imm(0));
			a->jz(skip); // cannot resolve function - SKIP

			// #2: Setup parameters

			Sub(*a, rsp, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ParamSize), 4)); // a->sub(rsp, function->NumParams * 8);

			if (CurrentStackDepth - 1) // apparently lea rsp, [rsp] doesn't encode
				a->lea(GetTempReg2(), ptr(rsp, (CurrentStackDepth - 1) * 8));
			else
				Move(*a, GetTempReg2(), rsp); // TempReg2 = virtual stack (RSP[CurrentStackDepth - 1])

			Add(*a, GetTempReg2(), ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ParamSize), 4)); // CurrentStackDepth += function->NumParams;

			Move(*a, r10d, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ParamSize), 4), 4); // i = numParams * 8;
			Sub(*a, r10d, asmjit::Imm(8), 4); // i -= 8;

			// ----- while (i != -8) {
			Compare(*a, r10d, asmjit::Imm(-8), 4);
			a->jz(skipParamLoop);
			a->bind(paramLoop);

			Move(*a, ptr(rsp, r10d), ptr(GetTempReg2()));

			Sub(*a, GetTempReg2(), asmjit::Imm(8)); // CurrentStackDepth--; (PopFromStack)
			Sub(*a, r10d, asmjit::Imm(8)); // i--;

			Compare(*a, r10d, asmjit::Imm(-8), 4);
			a->jnz(paramLoop);
			a->bind(skipParamLoop);
			// ----- }

			// #3: Call the function
			// TODO: what happens if an indirect function calls another indirect function?
			
			// store data
			Move(*a, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::StableStack)), GetStableStackReg());
			Move(*a, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::FakeStack)), GetTempReg2());
			a->push(GetTempReg3()); // store the helper result
			a->call(ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::Function)));
			a->pop(GetTempReg3());
			Move(*a, GetStableStackReg(), ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::StableStack)));
			Move(*a, GetTempReg2(), ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::FakeStack)));

			// #4: Setup return values

			Add(*a, rsp, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ParamSize))); // a->add(rsp, function->NumParams * 8);
			Sub(*a, GetTempReg2(), ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ParamSize))); // CurrentStackDepth -= function->NumParams;

			Move(*a, r10d, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ParamSize), 4), 4); // returnOffset = (function->NumParams * 8);
			Add(*a, r10d, asmjit::Imm(8 + 8)); // returnOffset += 8 (TempReg3); returnOffset += 8 (ReturnAddress);
			Add(*a, r10d, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ReturnSize), 4), 4); // returnOffset += (int)((int)function->NumReturns * 8);
			a->neg(r10d); // returnOffset = -returnOffset;
			a->movsxd(r10, r10d);

			Move(*a, r11d, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::NumReturns), 4)); // i = function->NumReturns;

			// ----- while (i != 0) {
			Compare(*a, r11d, asmjit::Imm(0), 4);
			a->jz(skipReturnLoop);
			a->bind(returnLoop);

			Add(*a, GetTempReg2(), asmjit::Imm(8)); // CurrentStackDepth++;
			Move(*a, ptr(GetTempReg2()), ptr(rsp, r10));
			Add(*a, r10, asmjit::Imm(8)); // returnOffset += 8;
			Sub(*a, r11d, asmjit::Imm(1), 4); // i--;

			Compare(*a, r11d, asmjit::Imm(0), 4);
			a->jnz(returnLoop);
			a->bind(skipReturnLoop);
			// ----- }

			// #5: Fix stack

			CurrentStackDepth += 1;
			Sub(*a, rsp, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ParamSize), 4)); // pop parameters
			Sub(*a, rsp, asmjit::Imm(8)); // pop function pointer
			Add(*a, rsp, ptr(GetTempReg3(), offsetof(CallIndirectResult, CallIndirectResult::ReturnSize), 4)); // push returns

			// #6: Whew

			a->bind(skip);

			break;
		}
		default:
		{
			throw std::runtime_error("Unhandled instruction");
		}
		}

		SeekIP(File->GetInsnSize(GetFileIP()));
	}

	// the stack will never be balanced if call indirect is used
	if (CurrentStackDepth != 0 && !CallIndirectUsed)
		throw std::runtime_error("Stack isn't balanced");

#ifndef _NDEBUG
	CheckLabels(a); // expensive check
#endif
}

