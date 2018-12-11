/*
 * linux/drivers/clk/clk.h
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct clk_hw;
struct clk_core;
struct device;
struct of_phandle_args;

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
int of_parse_clkspec(const struct device_node *np, int index, const char *name,
		     struct of_phandle_args *out_args);
struct clk_hw *of_clk_get_hw_from_clkspec(struct of_phandle_args *clkspec);
#endif

#ifdef CONFIG_COMMON_CLK
struct clk *clk_hw_create_clk(struct device *dev, struct clk_hw *hw,
			      const char *dev_id, const char *con_id);
void __clk_put(struct clk *clk);

/* Debugfs API to print the enabled clocks */
void clock_debug_print_enabled(void);
void clk_debug_print_hw(struct clk_core *clk, struct seq_file *f);

#else
/* All these casts to avoid ifdefs in clkdev... */
static inline struct clk *
clk_hw_create_clk(struct device *dev, struct clk_hw *hw, const char *dev_id,
		  const char *con_id)
{
	return (struct clk *)hw;
}
static struct clk_hw *__clk_get_hw(struct clk *clk)
{
	return (struct clk_hw *)clk;
}
static inline void __clk_put(struct clk *clk) { }

#endif
