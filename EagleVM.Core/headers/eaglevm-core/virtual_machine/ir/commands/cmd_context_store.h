#pragma once
#include "eaglevm-core/virtual_machine/ir/commands/base_command.h"

namespace eagle::ir
{
    class cmd_context_store : public base_command
    {
    public:
        explicit cmd_context_store(codec::reg dest);
        explicit cmd_context_store(codec::reg dest, codec::reg_size size);

        codec::reg get_reg() const;
        codec::reg_size get_value_size() const;

        bool is_similar(const std::shared_ptr<base_command>& other) override;

        std::string to_string() override;

    private:
        codec::reg dest;
        codec::reg_size size;
    };

    SHARED_DEFINE(cmd_context_store);
}
