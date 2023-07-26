#include "Runtime.h"
#include "Instruction.h"

#define GetRegVal(reg) m_Registers[reg.type]
#define TriggerFlags(op1, op2, res, bitsize)                        \
    m_Registers[RegType::ZF] = res == 0;                            \
    m_Registers[RegType::CF] = res < op1 || res < op2;              \
    m_Registers[RegType::SF] = (1 << bitsize - 1) & res;            \
    m_Registers[RegType::PF] = [](std::uint8_t num) -> std::uint8_t \
    {                                                               \
        auto n = 0;                                                 \
        while (num)                                                 \
        {                                                           \
            n += num & 1;                                           \
            num >>= 1;                                              \
        }                                                           \
        return n;                                                   \
    } (res << bitsize - 8) % 2 == 0;

#define Push8(u8)								\
	m_Sp--;										\
	*(std::uint8_t*)m_Sp = (std::uint8_t)u8
#define Push16(u16)								\
	m_Sp -= 2;									\
	*(std::uint16_t*)m_Sp = (std::uint16_t)u16
#define Push32(u32)								\
	m_Sp -= 4;									\
	*(std::uint32_t*)m_Sp = (std::uint32_t)u32
#define Pop8(u8)								\
	u8 = *(std::uint8_t*)m_Sp;					\
	m_Sp--
#define Pop16(u16)								\
	u16 = *(std::uint16_t*)m_Sp;				\
	m_Sp -= 2
#define Pop32(u32)								\
	u32 = *(std::uint32_t*)m_Sp;				\
	m_Sp -= 4
#define Pop8s()									\
	m_Sp--
#define Pop16s()								\
	m_Sp -= 2
#define Pop32s()								\
	m_Sp -= 4
#define ReadFrom(addr)							\
	*((std::uint8_t*)addr)
#define ReadFrom16(addr)						\
	*((std::uint16_t*)addr)
#define ReadFrom32(addr)						\
	*((std::uint32_t*)addr)
#define WriteAt(addr, u8)						\
	*((std::uint8_t*)addr) = (std::uint8_t)u8
#define WriteAt16(addr, u16)						\
	*((std::uint16_t*)addr) = (std::uint16_t)u16
#define WriteAt32(addr, u32)						\
	*((std::uint32_t*)addr) = (std::uint32_t)u32

namespace rlang::alvm {
	ALVM::ALVM(const std::vector<std::uint8_t>& data)
		: m_Stack(data), m_Sp(m_Registers[RegType::Sp])
	{
		m_Stack.resize(m_Stack.size() + STACK_SIZE);

		// Init registers
		m_Registers[RegType::Sp] = (std::uint32_t)m_Stack.data() + data.size() + STACK_SIZE;
		m_Registers[RegType::DS] = (std::uint32_t)m_Stack.data();
		m_Registers[RegType::SS] = (std::uint32_t)m_Stack.data() + data.size() - 1;
	}

	void ALVM::Run(const std::vector<Instruction>& code, std::int32_t& result)
	{
		m_Bytecode = ((std::vector<Instruction>&)code).data();
		m_Pc = m_Bytecode;

		m_Registers[RegType::CS] = (std::uint32_t)m_Bytecode;

		while (m_Pc)
			(this->*m_Instructions[(std::size_t)m_Pc->opcode])();

		result = m_Registers[RegType::R0];
	}

	void ALVM::Nop()
	{
		m_Pc++;
	}

	void ALVM::End()
	{
		m_Pc = nullptr;
	}

	void ALVM::Push()
	{
		switch (m_Pc->size)
		{
			case 8:
			{
				if (m_Pc->reg1.type != RegType::Nul)
				{
					Push8((std::uint8_t)m_Registers[m_Pc->reg1.type]);
				}
				else
				{
					Push8((std::uint8_t)m_Pc->imm32);
				}
				break;
			}
			case 16:
			{
				if (m_Pc->reg1.type != RegType::Nul)
				{
					Push16((std::uint16_t)m_Registers[m_Pc->reg1.type]);
				}
				else
				{
					Push16((std::uint16_t)m_Pc->imm32);
				}
				break;
			}
			case 32:
			default:
			{
				if (m_Pc->reg1.type != RegType::Nul)
				{
					Push32(m_Registers[m_Pc->reg1.type]);
				}
				else
				{
					Push32(m_Pc->imm32);
				}
				break;
			}
		}
		m_Pc++;
	}

	void ALVM::Pop()
	{
		switch (m_Pc->size)
		{
			case 8:
			{
				if (m_Pc->reg1.type != RegType::Nul)
				{
					Pop8(GetRegVal(m_Pc->reg1));
				}
				else
				{
					Pop8s();
				}
				break;
			}
			case 16:
			{
				if (m_Pc->reg1.type != RegType::Nul)
				{
					Pop16(GetRegVal(m_Pc->reg1));
				}
				else
				{
					Pop16s();
				}
				break;
			}        
			case 32:
			default:
			{
				if (m_Pc->reg1.type != RegType::Nul)
				{
					Pop32(GetRegVal(m_Pc->reg1));
				}
				else
				{
					Pop32s();
				}
				break;
			}
		}
		m_Pc++;
	}

	void ALVM::Add()
	{
		std::uint32_t op1, op2, res;

		if (m_Pc->reg1.type != RegType::Nul)
		{
			if (m_Pc->reg1.ptr)
			{
				// m, ...
				op1 = ReadFrom32(GetRegVal(m_Pc->reg1));
				if (m_Pc->reg2.type != RegType::Nul)
				{
					// m, imm32
					op2 = m_Pc->imm32;
				}
				else
				{
					// m, r
					op2 = GetRegVal(m_Pc->reg2);
				}
				res = op1 + op2;
				WriteAt32(GetRegVal(m_Pc->reg1), res);
			}
			else
			{
				// r, ...
				op1 = GetRegVal(m_Pc->reg1);
				if (m_Pc->reg2.type != RegType::Nul)
				{
					if (m_Pc->reg2.ptr)
					{
						// r, m
						op2 = ReadFrom32(GetRegVal(m_Pc->reg2));
					}
					else
					{
						// r, r
						op2 = GetRegVal(m_Pc->reg2);
					}
				}
				else
				{
					// r, imm32
					op2 = m_Pc->imm32;
				}

				res = op1 + op2;
				GetRegVal(m_Pc->reg1) = res;
			}
		}
		else
		{
			// r0, imm32
			op1 = m_Registers[RegType::R0];
			op2 = m_Pc->imm32;
			res = op1 + op2;
			m_Registers[RegType::R0] = res;
		}

		TriggerFlags(op1, op2, res, 32);
		m_Pc++;
	}

	void ALVM::Sub()
	{
		std::uint32_t op1, op2, res;

		if (m_Pc->reg1.type != RegType::Nul)
		{
			if (m_Pc->reg1.ptr)
			{
				// m, ...
				op1 = ReadFrom32(GetRegVal(m_Pc->reg1));
				if (m_Pc->reg2.type != RegType::Nul)
				{
					// m, imm32
					op2 = m_Pc->imm32;
				}
				else
				{
					// m, r
					op2 = GetRegVal(m_Pc->reg2);
				}
				res = op1 - op2;
				WriteAt32(GetRegVal(m_Pc->reg1), res);
			}
			else
			{
				// r, ...
				op1 = GetRegVal(m_Pc->reg1);
				if (m_Pc->reg2.type != RegType::Nul)
				{
					if (m_Pc->reg2.ptr)
					{
						// r, m
						op2 = ReadFrom32(GetRegVal(m_Pc->reg2));
					}
					else
					{
						// r, r
						op2 = GetRegVal(m_Pc->reg2);
					}
				}
				else
				{
					// r, imm32
					op2 = m_Pc->imm32;
				}

				res = op1 - op2;
				GetRegVal(m_Pc->reg1) = res;
			}
		}
		else
		{
			// r0, imm32
			op1 = m_Registers[RegType::R0];
			op2 = m_Pc->imm32;
			res = op1 - op2;
			m_Registers[RegType::R0] = res;
		}

		TriggerFlags(op1, op2, res, 32);
		m_Pc++;
	}

	void ALVM::Mul()
	{
		std::uint32_t op1 = m_Registers[RegType::R0], op2;
		if (m_Pc->reg1.ptr)
		{
			// r0, m
			op2 = ReadFrom32(GetRegVal(m_Pc->reg1));
		}
		else
		{
			// r0, r
			op2 = GetRegVal(m_Pc->reg1);
		}
		m_Registers[RegType::R0] = op1 * op2;
		m_Pc++;
	}

	void ALVM::Div()
	{
		std::uint32_t op1 = m_Registers[RegType::R0], op2;
		if (m_Pc->reg1.ptr)
		{
			// r, m
			op2 = ReadFrom32(GetRegVal(m_Pc->reg1));
		}
		else
		{
			// r, r
			op2 = GetRegVal(m_Pc->reg1);
		}

		m_Registers[RegType::R0] = op1 / op2;
		m_Registers[RegType::R3] = op1 % op2;
		m_Pc++;
	}

	void ALVM::Increment()
	{
		if (m_Pc->reg1.ptr)
		{
			// m
			switch (m_Pc->size)
			{
				case 8:
				{
					std::uint8_t op1 = ReadFrom(GetRegVal(m_Pc->reg1)), res = ++op1;
					TriggerFlags(op1, 1, res, 32);
					break;
				}
				case 16:
				{
					std::uint16_t op1 = ReadFrom16(GetRegVal(m_Pc->reg1)), res = ++op1;
					TriggerFlags(op1, 1, res, 32);
					break;
				}
				case 32:
				{
					std::uint32_t op1 = ReadFrom32(GetRegVal(m_Pc->reg1)), res = ++op1;
					TriggerFlags(op1, 1, res, 32);
					break;
				}
			}
		}
		else
		{
			// r
		}
	}

	void ALVM::Decrement()
	{

	}

	void ALVM::PrintInt()
	{
		if (m_Pc->reg1.ptr)
		{
			switch (m_Pc->size)
			{
				case 8:
				{
					//std::printf("%d", ReadFrom((std::size_t)m_Registers[m_Pc->reg1.type]));
					std::cout << ReadFrom((std::size_t)m_Registers[m_Pc->reg1.type]);
					break;
				}
				case 16:
				{
					//std::printf("%d", ReadFrom16((std::size_t)m_Registers[m_Pc->reg1.type]));
					std::cout << ReadFrom16((std::size_t)m_Registers[m_Pc->reg1.type]);
					break;
				}
				case 32:
				{
					//std::printf("%d", ReadFrom32((std::size_t)m_Registers[m_Pc->reg1.type]));
					std::cout << ReadFrom32((std::size_t)m_Registers[m_Pc->reg1.type]);
					break;
				}
			}
		}
		else
		{
			//std::printf("%d", m_Registers[m_Pc->reg1.type]);
			std::cout << m_Registers[m_Pc->reg1.type];
		}
		m_Pc++;
	}

	void ALVM::PrintStr()
	{
		// FIXME: Buffer the string instead of making a function for every single character.
		std::uint8_t c = 1;
		std::size_t i = 0;
		while ((c = ReadFrom(m_Registers[m_Pc->reg1.type] + i++)) != 0)
			std::cout << c;

		m_Pc++;
	}

    void ALVM::Cmp()
    {
        m_Pc++;
    }

	void ALVM::Move()
	{
		// this can probably be refactored to be better but meh
		if (m_Pc->reg2.type != RegType::Nul)
		{
			if (m_Pc->reg2.ptr)
			{
				if (m_Pc->reg1.ptr)
				{
					switch (m_Pc->reg1.size)
					{
						case 8:
							WriteAt(m_Registers[m_Pc->reg1.type], ReadFrom(m_Registers[m_Pc->reg2.type]));
							break;
						case 16:
							switch (m_Pc->reg2.size)
							{
								case 8:
									WriteAt16(m_Registers[m_Pc->reg1.type], ReadFrom(m_Registers[m_Pc->reg2.type]));
									break;
								case 16:
									WriteAt16(m_Registers[m_Pc->reg1.type], ReadFrom16(m_Registers[m_Pc->reg2.type]));
									break;
							}
							break;
						case 32:
						default:
							switch (m_Pc->reg2.size)
							{
								case 8:
									WriteAt32(m_Registers[m_Pc->reg1.type], ReadFrom(m_Registers[m_Pc->reg2.type]));
									break;
								case 16:
									WriteAt32(m_Registers[m_Pc->reg1.type], ReadFrom16(m_Registers[m_Pc->reg2.type]));
									break;
								case 32:
								default:
									WriteAt32(m_Registers[m_Pc->reg1.type], ReadFrom32(m_Registers[m_Pc->reg2.type]));
									break;
							}
							break;
					}
				}
				else
				{
					switch (m_Pc->reg1.size)
					{
						case 8:
							m_Registers[m_Pc->reg1.type] = ReadFrom(m_Registers[m_Pc->reg2.type]);
							break;
						case 16:
							m_Registers[m_Pc->reg1.type] = ReadFrom16(m_Registers[m_Pc->reg2.type]);
							break;
						case 32:
						default:
							m_Registers[m_Pc->reg1.type] = ReadFrom32(m_Registers[m_Pc->reg2.type]);
							break;
					}
				}
			}
			else
			{
				if (m_Pc->reg1.ptr)
				{
					switch (m_Pc->reg1.size)
					{
						case 8:
							WriteAt(m_Registers[m_Pc->reg1.type], (std::uint8_t)m_Registers[m_Pc->reg2.type]);
							break;
						case 16:
							WriteAt16(m_Registers[m_Pc->reg1.type], (std::uint16_t)m_Registers[m_Pc->reg2.type]);
							break;
						case 32:
						default:
							WriteAt32(m_Registers[m_Pc->reg1.type], m_Registers[m_Pc->reg2.type]);
							break;
					}
				}
				else
				{
					m_Registers[m_Pc->reg1.type] = m_Registers[m_Pc->reg2.type];
				}
			}
		}
		else
		{
			if (m_Pc->reg1.ptr)
			{
				switch (m_Pc->reg1.size)
				{
					case 8:
						WriteAt(m_Registers[m_Pc->reg1.type], (std::uint8_t)m_Pc->imm32);
						break;
					case 16:
						WriteAt16(m_Registers[m_Pc->reg1.type], (std::uint16_t)m_Pc->imm32);
						break;
					case 32:
					default:
						WriteAt32(m_Registers[m_Pc->reg1.type], m_Pc->imm32);
						break;
				}
			}
			else
			{
				m_Registers[m_Pc->reg1.type] = m_Pc->imm32;
			}
		}
		m_Pc++;
	}

	void ALVM::Jump()
	{
		if (m_Pc->reg1.type != RegType::Nul) m_Pc = m_Bytecode + m_Registers[m_Pc->reg1.type];
		else m_Pc = m_Bytecode + m_Pc->imm32;
	}

	void ALVM::ConditionalJump()
	{
		if (m_Registers[RegType::CF]) Jump();
		else m_Pc++;
	}

	void ALVM::ConditionalNotJump()
	{
		if (!m_Registers[RegType::CF]) Jump();
		else m_Pc++;
	}

	void ALVM::Call()
	{
		Push32((std::uintptr_t)(m_Pc + 1));
		Jump();
	}

	void ALVM::Return()
	{
		Pop32(std::uint32_t addr);
		//Instruction* addr = (Instruction*)(std::uintptr_t)Pop32();
		m_Pc = (Instruction*)addr;
	}

	void ALVM::Malloc()
	{
		if (m_Pc->reg1.type != RegType::Nul)
		{
			if (m_Pc->reg1.ptr)
			{
				switch (m_Pc->size)
				{
					case 8:
						m_Registers[RegType::R0] = (std::uint32_t)std::malloc((std::size_t)ReadFrom(GetRegVal(m_Pc->reg1)));
						break;
					case 16:
						m_Registers[RegType::R0] = (std::uint32_t)std::malloc((std::size_t)ReadFrom16(GetRegVal(m_Pc->reg1)));
						break;
					case 32:
						m_Registers[RegType::R0] = (std::uint32_t)std::malloc((std::size_t)ReadFrom32(GetRegVal(m_Pc->reg1)));
						break;
				}
			}
			else
			{
				m_Registers[RegType::R0] = (std::uint32_t)std::malloc((std::size_t)GetRegVal(m_Pc->reg1));
			}
		}
		else
		{
			m_Registers[RegType::R0] = (std::uint32_t)std::malloc((std::size_t)m_Pc->imm32);
		}
		m_Pc++;
	}

	void ALVM::Free()
	{
		// So dodgy...
		free((void*)GetRegVal(m_Pc->reg1));
		m_Pc++;
	}

	/*void ALVM::Db()
	{
		std::size_t size = m_Pc->bytes.size();
		InsertBack(m_Pc->bytes.begin(), m_Pc->bytes.end());
		m_Registers[RegType::Sp] -= size;
		m_Pc++;
	}*/
} // namespace rlang::alvm
