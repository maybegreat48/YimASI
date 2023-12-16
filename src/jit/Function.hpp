#pragma once
#include "Header.hpp"
#include <unordered_set>

namespace JIT
{
	class ScriptFile;

	struct ConstantPushData
	{
		int Constant = 0;
		int RelAddr  = 0;
		bool Valid   = false;
		int Index    = 0;
	};

	struct ConstantArgData
	{
		std::optional<int> ConstantArg1 = std::nullopt;
		std::optional<int> ConstantArg2 = std::nullopt;
	};

	struct StackSnapshot
	{
		int Depth = -1;
		int Delta            = -1;
		bool InterruptHill = false;
		bool CanUseRegisters = false;
		ConstantArgData ConstantArgData{};
		bool Skip = false;
		bool SuppressPushes[3]{};
	};

	class Function
	{
		ScriptFile* File{};

		// labels
		std::unordered_map<int, asmjit::Label> Labels{}; // relative

		// virtual stack
		int HighestPeak{};
		bool CallIndirectUsed{};
		int NumCallIndirects{};
		std::map<int, StackSnapshot> StackSnapshots{}; // relative
		std::vector<ConstantPushData> FakeStack{};
		void MarkPushHandled(ConstantPushData& data);
		void ProcessStackHills();
		int GetStackPeak();

		// locals
		asmjit::Operand GetParameter(int index, int size = 8);
		asmjit::Operand GetLocalFrame(int index, int size = 8);
		asmjit::Operand GetLocal(int index, int size = 8);
		asmjit::Operand GetReturn(int index, int size = 8);

		// assembler
		int CurrentInstructionPointer{}; // relative
		int CurrentStackDepth{};
		asmjit::BaseNode* FirstNode;
		void SeekIP(int value);
		int GetRelIP();
		int GetFileIP();
		void CheckLabels(Builder* a);
		bool UsingRegisters();

		asmjit::Operand GetStackItem(int index, int size = 8);
		asmjit::Operand PushToStack(int size = 8);
		asmjit::Operand PopFromStack(int size = 8);
		asmjit::Operand GetStackTop(int size = 8);

	public:
		Function(ScriptFile* file, int begin, int end, int params, int locals, int returns);

		/// <summary>
		/// Registers jump targets to look out for in the assembly phase
		/// </summary>
		void Preprocess(Builder* a);

		/// <summary>
		/// Assembles code into the assembler
		/// </summary>
		/// <param name="a"></param>
		void Assemble(Builder* a);
		
		int Id{};
		int CodeStartOffset{}; // before ENTER
		int CodeEndOffset{};
		asmjit::Label FunctionLabel{};
		int NumParams{};
		int NumLocals{};
		int NumReturns{};
		int Xrefs{};
	};
}