#pragma once
#include "eaglevm-core/codec/zydis_helper.h"
#include "eaglevm-core/virtual_machine/ir/models/ir_store.h"
#include "eaglevm-core/virtual_machine/ir/commands/base_command.h"
#include "eaglevm-core/virtual_machine/ir/dynamic_encoder/encoder.h"

namespace eagle::ir
{
    class cmd_x86_dynamic : public base_command
    {
    public:
        template <typename... Ops>
        explicit cmd_x86_dynamic(const encoder::encoder& encoder)
            : base_command(command_type::vm_exec_dynamic_x86), encoder(encoder)
        {
        }

        encoder::encoder& get_encoder();
        bool is_similar(const std::shared_ptr<base_command>& other) override;

        std::vector<discrete_store_ptr> get_use_stores() override;

    private:
        encoder::encoder encoder;
    };

    template <typename... Operands>
    std::shared_ptr<cmd_x86_dynamic> make_dyn(const codec::mnemonic mnemonic, Operands... ops)
    {
        encoder::encoder enc{ mnemonic };
        (enc.add_operand(ops), ...);

        return std::make_shared<cmd_x86_dynamic>(enc);
    }

    SHARED_DEFINE(cmd_x86_dynamic);
}
