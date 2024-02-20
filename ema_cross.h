#pragma once

#include "strategy_interface.h"

namespace ema_cross
{

struct param_s
{
    int fast_period;
    int slow_period;

    int timeout;
};

class strategy_c : public strategy_interface_c
{
public:
    strategy_error_e init(const strategy_data_s& data) override;
    strategy_error_e run(strategy_result_s& result, const strategy_log_e log = strategy_log_e::All) override;
    strategy_error_e update() override;
private:
    param_s param;

    int begin_idx;
    int skip;
    int nb_element;

    std::vector<double> ema_fast;
    std::vector<double> ema_slow;
};

} // namespace ema_cross
