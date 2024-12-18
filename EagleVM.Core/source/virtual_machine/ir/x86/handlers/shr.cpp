#include "eaglevm-core/virtual_machine/ir/x86/handlers/shr.h"
#include "eaglevm-core/virtual_machine/ir/x86/handlers/util/flags.h"

#include "eaglevm-core/virtual_machine/ir/commands/cmd_context_rflags_load.h"
#include "eaglevm-core/virtual_machine/ir/commands/cmd_context_rflags_store.h"

#include "eaglevm-core/virtual_machine/ir/block_builder.h"

namespace eagle::ir::handler
{
    shr::shr()
    {
        valid_operands = {
            { { { codec::op_none, codec::bit_8 }, { codec::op_none, codec::bit_8 } }, "shr 8,8" },
            { { { codec::op_none, codec::bit_16 }, { codec::op_none, codec::bit_16 } }, "shr 16,16" },
            { { { codec::op_none, codec::bit_32 }, { codec::op_none, codec::bit_32 } }, "shr 32,32" },
            { { { codec::op_none, codec::bit_64 }, { codec::op_none, codec::bit_64 } }, "shr 64,64" },

            // sign extended handlers
            { { { codec::op_none, codec::bit_16 }, { codec::op_imm, codec::bit_8 } }, "shr 16,16" },
            { { { codec::op_none, codec::bit_32 }, { codec::op_imm, codec::bit_8 } }, "shr 32,32" },
            { { { codec::op_none, codec::bit_64 }, { codec::op_imm, codec::bit_8 } }, "shr 64,64" },
            { { { codec::op_none, codec::bit_64 }, { codec::op_imm, codec::bit_32 } }, "shr 64,64" },
        };

        build_options = {
            { { ir_size::bit_8, ir_size::bit_8 }, "shr 8,8" },
            { { ir_size::bit_16, ir_size::bit_16 }, "shr 16,16" },
            { { ir_size::bit_32, ir_size::bit_32 }, "shr 32,32" },
            { { ir_size::bit_64, ir_size::bit_64 }, "shr 64,64" },
        };
    }

    ir_insts shr::gen_handler(handler_sig signature)
    {
        VM_ASSERT(signature.size() == 2, "invalid signature. must contain 2 operands");
        VM_ASSERT(signature[0] == signature[1], "invalid signature. must contain same sized parameters");

        const ir_size target_size = signature.front();

        // the way this is done is far slower than it used to be
        // however because of the way this IL is written, there is far more room to expand how the virtual context is stored
        // in shrition, it gives room for mapping x86 context into random places as well

        // todo: some kind of virtual machine implementation where it could potentially try to optimize a pop and use of the register in the next
        // instruction using stack dereference
        constexpr auto affected_flags = ZYDIS_CPUFLAG_CF | ZYDIS_CPUFLAG_OF | ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_ZF | ZYDIS_CPUFLAG_PF;
        block_builder builder;
        builder
            .add_shr(target_size, false, true)

            /*
                The CF flag contains the value of the last bit shifted out of the destination operand; it is undefined for SHL and SHR instructions
                where the count is greater than or equal to the size (in bits) of the destination operand. The OF flag is affected only for 1-bit
                shifts (see “Description” above); otherwise, it is undefined. The SF, ZF, and PF flags are set according to the result. If the count is
                0, the flags are not affected. For a non-zero count, the AF flag is undefined.
            */

            // CF, OF, SF, ZF, PF are all set
            .add_context_rflags_load()
            .add_push(~affected_flags, ir_size::bit_64)
            .add_and(ir_size::bit_64)

            .append(compute_of(target_size))
            .append(compute_cf(target_size))

            .append(util::calculate_sf(target_size))
            .append(util::calculate_zf(target_size))
            .append(util::calculate_pf(target_size));

        return builder.build();
    }

    ir_insts shr::compute_cf(ir_size size)
    {
        //
        // CF = (value >> (shift_count - 1))
        //

        ir_insts insts;
        insts.append_range(copy_to_top(size, util::result));

        // (shift_count & 3F/1F)
        insts.append_range(copy_to_top(size, util::param_one, { size }));
        if (size == ir_size::bit_64)
            insts.push_back(std::make_shared<cmd_push>(0x3F, size));
        else
            insts.push_back(std::make_shared<cmd_push>(0x1F, size));

        insts.push_back(std::make_shared<cmd_and>(size));

        return {
            // result = value >> (shift_count - 1)
            std::make_shared<cmd_push>(1, size),
            std::make_shared<cmd_sub>(size),
            std::make_shared<cmd_shl>(size),

            // CF = result & 1
            std::make_shared<cmd_push>(1, size),
            std::make_shared<cmd_and>(size),

            std::make_shared<cmd_resize>(size, ir_size::bit_64),
            std::make_shared<cmd_push>(util::flag_index(ZYDIS_CPUFLAG_CF), ir_size::bit_64),
            std::make_shared<cmd_shl>(ir_size::bit_64),

            std::make_shared<cmd_or>(ir_size::bit_64),
        };
    }

    ir_insts shr::compute_of(ir_size size)
    {
        //
        // OF = MSB(tempDEST)
        //

        ir_insts insts;
        insts.append_range(copy_to_top(size, util::param_one));
        insts.append_range(ir_insts{
            std::make_shared<cmd_push>(static_cast<uint64_t>(size) - 1, size),
            std::make_shared<cmd_shr>(size),
            std::make_shared<cmd_push>(1, size),
            std::make_shared<cmd_and>(size),
            std::make_shared<cmd_resize>(size, ir_size::bit_64),

            std::make_shared<cmd_push>(util::flag_index(ZYDIS_CPUFLAG_OF), ir_size::bit_64),
            std::make_shared<cmd_shl>(ir_size::bit_64),

            std::make_shared<cmd_or>(ir_size::bit_64),
        });

        return insts;
    }
}

namespace eagle::ir::lifter
{
    translate_mem_result shr::translate_mem_action(const codec::dec::op_mem& op_mem, uint8_t idx)
    {
        return idx == 0 ? translate_mem_result::both : base_x86_translator::translate_mem_action(op_mem, idx);
    }

    translate_status shr::encode_operand(codec::dec::op_imm op_imm, uint8_t)
    {
        codec::dec::operand second_op = operands[1];
        codec::dec::operand first_op = operands[0];

        ir_size imm_size = static_cast<ir_size>(second_op.size);
        ir_size imm_size_target = static_cast<ir_size>(first_op.size);

        block->push_back(std::make_shared<cmd_push>(op_imm.value.u, imm_size));
        if (imm_size != imm_size_target)
            block->push_back(std::make_shared<cmd_sx>(imm_size_target, imm_size));

        return translate_status::success;
    }

    void shr::finalize_translate_to_virtual(const x86_cpu_flag flags)
    {
        base_x86_translator::finalize_translate_to_virtual(flags);

        codec::dec::operand first_op = operands[0];
        if (first_op.type == ZYDIS_OPERAND_TYPE_REGISTER)
        {
            // register
            codec::reg reg = static_cast<codec::reg>(first_op.reg.value);
            if (static_cast<ir_size>(first_op.size) == ir_size::bit_32)
            {
                reg = codec::get_bit_version(first_op.reg.value, codec::gpr_64);
                block->push_back(std::make_shared<cmd_resize>(ir_size::bit_64, ir_size::bit_32));
                block->push_back(std::make_shared<cmd_context_store>(reg));
            }
            else
            {
                block->push_back(std::make_shared<cmd_context_store>(reg));
            }

            if (reg == codec::rsp)
                return;

            // clean up regs on stack due to handler leaving params
            const ir_size target_size = static_cast<ir_size>(first_op.size);
            block->push_back(std::make_shared<cmd_pop>(target_size));
            block->push_back(std::make_shared<cmd_pop>(target_size));
        }
        else if (first_op.type == ZYDIS_OPERAND_TYPE_MEMORY)
        {
            // carry down result

            ir_size value_size = static_cast<ir_size>(first_op.size);
            block->push_back(std::make_shared<cmd_carry>(value_size, operands[1].size / 8 * 2));
            block->push_back(std::make_shared<cmd_mem_write>(value_size, value_size, true));
        }
    }
}