#ifndef ALVM_ALA_COMPILER_H
#define ALVM_ALA_COMPILER_H

#include <unordered_map>
#include <cstdint>
#include <ALVM.h>

#include "Lexer.h"
#include "Utils.h"

namespace rlang::rmc {
	struct DataInfo
	{
		std::size_t addr = 0;
		std::size_t size = 0;
		std::uint32_t value = 0;
		TokenType type = TokenType::Whitespace;
	};

	struct Compiler
	{
	private:
		static std::unordered_map<std::string, DataInfo> m_DataNameTable;
		static std::unordered_map<std::string, std::pair<std::size_t, std::unordered_map<std::string, std::size_t>>> m_LabelAddressMap;
		static std::vector<alvm::Instruction> m_CompiledCode;
		static std::vector<alvm::Instruction> m_InstEpilogue;
		static std::vector<std::uint8_t> m_DataSection;
		static std::string m_CurrentSection;
		static std::size_t m_StackSize;
		static std::size_t m_DataCount;

	private:
		static void Preproccess(const TokenList& tokens);
		static void Cleanup();

	public:
		static std::pair<std::vector<alvm::Instruction>, std::vector<std::uint8_t>> Compile(const TokenList& tokens);
		
	private:
		static alvm::OpCode GetInst(std::string inst);
		static alvm::RegType GetReg(std::string reg);
	};
}

#endif // ALVM_ALA_COMPILER_H
