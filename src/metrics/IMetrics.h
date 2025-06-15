#pragma once

#include "prometheus/registry.h"
#include "prometheus/exposer.h"
#include "prometheus/gauge.h"

class IMetrics {
public:
    virtual ~IMetrics() = default;
    virtual void register_metrics(prometheus::Registry& registry) = 0;
};