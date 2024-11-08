// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013-2020 Minetest core developers team

#pragma once
#include <memory>
#include <string>
#include <utility>
#include "config.h"

class MetricCounter
{
public:
	MetricCounter() = default;

	virtual ~MetricCounter() {}

	virtual void increment(double number = 1.0) = 0;
	virtual double get() const = 0;
};

typedef std::shared_ptr<MetricCounter> MetricCounterPtr;

class MetricGauge
{
public:
	MetricGauge() = default;
	virtual ~MetricGauge() {}

	virtual void increment(double number = 1.0) = 0;
	virtual void decrement(double number = 1.0) = 0;
	virtual void set(double number) = 0;
	virtual double get() const = 0;
};

typedef std::shared_ptr<MetricGauge> MetricGaugePtr;

class MetricsBackend
{
public:
	MetricsBackend() = default;

	virtual ~MetricsBackend() {}

	typedef std::initializer_list<std::pair<const std::string, std::string>> Labels;

	virtual MetricCounterPtr addCounter(
			const std::string &name, const std::string &help_str,
			Labels labels = {});
	virtual MetricGaugePtr addGauge(
			const std::string &name, const std::string &help_str,
			Labels labels = {});
};

#if USE_PROMETHEUS
MetricsBackend *createPrometheusMetricsBackend();
#endif
