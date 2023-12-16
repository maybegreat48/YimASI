#pragma once
#include <script/scrProgram.hpp>
#include "Header.hpp"
#include "Context.hpp"
#include "Function.hpp"
#include "Instruction.hpp"

namespace JIT
{
	class ScriptFile
	{
	private:
		Opcode GetOpcode(uint32_t pos);
		uint32_t GetInsnSize(uint32_t pos);
		uint32_t GetInsnOperand(uint32_t pos, uint8_t op);
		int32_t GetInsnOperandSigned(uint32_t pos, uint8_t op);
		uint32_t GetInsnRelJmpAddr(uint32_t pos);

		asmjit::CodeHolder Code;
		std::unique_ptr<asmjit::x86::Builder> Builder;
		asmjit::StringLogger Logger;
		main_t MainFunction = nullptr;
		bool CallIndirectUsed = false;
		rage::scrProgram* Program;
		std::map<uint64_t, std::unique_ptr<Function>> Functions;
		std::vector<uint64_t> Natives;
		int HighestUnlinkedReturnCount;

		/// <summary>
		/// Attempts to calculate a safe stack offset using a simple heuristic
		/// </summary>
		void ProcessCallIndirectRetCount();

	public:
		ScriptFile(rage::scrProgram* program);

		/// <summary>
		/// Create function objects
		/// </summary>
		void OutlineProgram();

		/// <summary>
		/// Assembles all functions
		/// </summary>
		void AssembleProgram();

		/// <summary>
		/// Get the main function
		/// </summary>
		/// <returns>The main function</returns>
		main_t GetMainFunction();

		/// <summary>
		/// Gets the function at <b>address</b> if it exists, nullptr otherwise
		/// </summary>
		/// <param name="address"></param>
		/// <returns></returns>
		Function* GetFunctionAtAddress(uint32_t address);

		/// <summary>
		/// Gets the code holder for the script file
		/// </summary>
		/// <returns>The code holder</returns>
		asmjit::CodeHolder& GetCode();

		friend Function;
	};
}