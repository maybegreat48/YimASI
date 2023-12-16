#include "ScriptFile.hpp"
#include "Instruction.hpp"

#include "Instruction.hpp"

namespace
{
	uint32_t inline Read24(uint8_t* arr)
	{
		return arr[0] + (arr[1] << 8) + (arr[2] << 16);
	}

	uint32_t inline ScriptRVA(int16_t offset, uint32_t ip)
	{
		return ip + (int)(2 + offset + 1);
	};

	class ThrowableErrorHandler : public asmjit::ErrorHandler
	{
	public:
		void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override
		{
		  throw std::exception(message);
		}
	};
}

JIT::Opcode JIT::ScriptFile::GetOpcode(uint32_t pos)
{
	return Opcode(*Program->get_code_address(pos));
}

uint32_t JIT::ScriptFile::GetInsnSize(uint32_t pos)
{
	uint8_t opcode = *Program->get_code_address(pos);
	uint32_t size  = 1;

	auto& params = g_OpcodeArgs[opcode].second;
	for (size_t i = 0; i < params.first.length(); ++i)
	{
		switch (params.first[i])
		{
		case '$': size += *Program->get_code_address(pos + size) + 1; break;
		case 'R': size += 2; break;
		case 'S': size += ((size_t)(*Program->get_code_address(pos + size)) * 6 + 1); break;
		case 'a': size += 3; break;
		case 'b': size += 1; break;
		case 'd':
		case 'f': size += 4; break;
		case 'h':
		case 's': size += 2; break;
		}
	}

	return size;
}

uint32_t JIT::ScriptFile::GetInsnOperand(uint32_t pos, uint8_t op)
{
	uint8_t opcode = *Program->get_code_address(pos);
	if (op >= g_OpcodeArgs[opcode].second.first.length())
		throw std::runtime_error("operand index out of range");

	auto& params = g_OpcodeArgs[opcode].second.first;
	int offset   = 1;
	for (int i = 0; i < op; i++)
	{
		switch (params[i])
		{
		case '$': offset += *Program->get_code_address(pos + offset) + 1; break;
		case 'R': offset += 2; break;
		case 'S': offset += ((size_t)(*Program->get_code_address(pos + offset)) * 6 + 1); break;
		case 'a': offset += 3; break;
		case 'b': offset += 1; break;
		case 'd':
		case 'f': offset += 4; break;
		case 'h':
		case 's': offset += 2; break;
		}
	}

	switch (g_OpcodeArgs[opcode].second.first[op])
	{
	case 'b': return *Program->get_code_address(pos + offset);
	case 's':
	case 'h': return *(std::uint16_t*)Program->get_code_address(pos + offset);
	case 'a': return Read24(Program->get_code_address(pos + offset));
	case 'f':
	case 'd': return *(std::uint32_t*)Program->get_code_address(pos + offset);
	}

	return 0;
}

int32_t JIT::ScriptFile::GetInsnOperandSigned(uint32_t pos, uint8_t op)
{
	uint8_t opcode = *Program->get_code_address(pos);
	if (op >= g_OpcodeArgs[opcode].second.first.length())
		throw std::runtime_error("operand index out of range");

	auto& params = g_OpcodeArgs[opcode].second.first;
	int offset   = 1;
	for (int i = 0; i < op; i++)
	{
		switch (params[i])
		{
		case '$': offset += *Program->get_code_address(pos + offset) + 1; break;
		case 'R': offset += 2; break;
		case 'S': offset += ((size_t)(*Program->get_code_address(pos + offset)) * 6 + 1); break;
		case 'a': offset += 3; break;
		case 'b': offset += 1; break;
		case 'd':
		case 'f': offset += 4; break;
		case 'h':
		case 's': offset += 2; break;
		}
	}

	switch (g_OpcodeArgs[opcode].second.first[op])
	{
	case 'b': return *(std::int8_t*)Program->get_code_address(pos + offset);
	case 's':
	case 'h': return *(std::int16_t*)Program->get_code_address(pos + offset);
	case 'a': return (std::int32_t)Read24(Program->get_code_address(pos + offset));
	case 'f':
	case 'd': return *(std::int32_t*)Program->get_code_address(pos + offset);
	}

	return 0;
}

uint32_t JIT::ScriptFile::GetInsnRelJmpAddr(uint32_t pos)
{
	return ScriptRVA((*(std::int16_t*)Program->get_code_address(pos + 1)), pos);
}


void JIT::ScriptFile::ProcessCallIndirectRetCount()
{
	for (auto& [_, func] : Functions)
		if (func->Xrefs == 0)
		  HighestUnlinkedReturnCount = std::max(HighestUnlinkedReturnCount, func->NumReturns);
}

JIT::ScriptFile::ScriptFile(rage::scrProgram* program) :
	Program(program)
{
	static ThrowableErrorHandler eh;

	Code.init(g_JIT.environment(), g_JIT.cpuFeatures());
	Code.setLogger(&Logger);
	Code.setErrorHandler(&eh);

	Builder = std::make_unique<asmjit::x86::Builder>(&Code);
}

void JIT::ScriptFile::OutlineProgram()
{
	int ip = 5; // skip the initial ENTER
	int curFuncStart = 0; // the start of the function; doesn't have to be on the ENTER insn
	int curFuncBegin = 0; // the ENTER insn
	int lastLeaveOp  = 0;
	while (ip < Program->m_code_size)
	{
		if (*Program->get_code_address(ip) == (uint8_t)Opcode::LEAVE)
		{
			lastLeaveOp = ip;
		}
		else if (*Program->get_code_address(ip) == (uint8_t)Opcode::ENTER)
		{
			// reached the next function
			Functions[curFuncStart] = std::make_unique<Function>(this, curFuncStart, lastLeaveOp + GetInsnSize(lastLeaveOp), GetInsnOperand(curFuncBegin, 0), GetInsnOperand(curFuncBegin, 1), GetInsnOperand(lastLeaveOp, 1));
			curFuncStart = lastLeaveOp + GetInsnSize(lastLeaveOp);
			curFuncBegin = ip;
		}
		ip += GetInsnSize(ip);
	}
	Functions[curFuncStart] = std::make_unique<Function>(this, curFuncStart, lastLeaveOp + GetInsnSize(lastLeaveOp), GetInsnOperand(curFuncBegin, 0), GetInsnOperand(curFuncBegin, 1), GetInsnOperand(lastLeaveOp, 1)); // add the last function
}

void JIT::ScriptFile::AssembleProgram()
{
	for (auto& [_, f] : Functions)
		f->Preprocess(Builder.get());
	if (CallIndirectUsed)
		ProcessCallIndirectRetCount();
	for (auto& [_, f] : Functions)
		f->Assemble(Builder.get());
}

JIT::main_t JIT::ScriptFile::GetMainFunction()
{
	if (!MainFunction)
	{
		Builder->finalize();

		asmjit::String sb;
		asmjit::FormatOptions formatOptions{};

		asmjit::Formatter::formatNodeList(sb, formatOptions, Builder.get());
		g_JIT.add(&MainFunction, &Code);

		std::ofstream file("file.bin", std::ios::binary);
		file.write((const char*)MainFunction, Code.codeSize());
		file.close();

		std::ofstream assm("file.asm");
		assm.write(sb.data(), sb.size());
		assm.close();
	}

	return MainFunction;
}

JIT::Function* JIT::ScriptFile::GetFunctionAtAddress(uint32_t address)
{
	if (Functions.count(address))
		return Functions.at(address).get();
	return nullptr;
}

asmjit::CodeHolder& JIT::ScriptFile::GetCode()
{
	return Code;
}
