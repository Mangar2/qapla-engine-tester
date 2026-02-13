#pragma once

/**
 * Returns the number of physical CPU cores.
 * This count excludes virtual cores provided by Hyper-Threading or SMT.
 * * @return Number of physical cores, or 0 if detection fails.
 */
int getPhysicalCoreCount();