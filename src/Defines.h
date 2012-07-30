#pragma once

//our defines

#define __REAVER_FIRST__

#define __DEBUG__
#define __INTELLIGENCE_DEBUG__
#define __ETECHESTIMATOR_DEBUG__
#define __MACRO_DEBUG__
#define __MICRO_DEBUG__
#define __ARBITRATOR_DEBUG__

//#define __NON_IMPLEMENTE__
///#define BW_POS_MOUSE
//#define __LEARNING_PROB_TABLES__

#define WALK_TILES_SIZE 8

#define __DO_NOT_HARASS_SCOUT__ // will not compile the harassing part in the FirstScoutGoal (to play against the built-in AI)
#define __MANNER__ // will say gl hf (and in the future TODO leave game)
#define __CONTROL_BO_UNTIL_SECOND_PYLON__ // no supply management before the second pylon, if you don't place it in the BO, you're locked
#define __ETECH_ESTIMATOR__ // use the ETechEstimator (or not if undef)
#define __BENS_LABELS__ // use Ben Weber's openings/strategies labels in the ETechEstimator
#define __WORKERS_FACTOR__ 1.12 // how much more workers we want on top of the optimal count (nb workers = optimal * __WORKERS_FACTOR__)
#define __MAX_WORKERS__ 75
#define __TILES_RADIUS_DEFEND_BASE__ 26
#define __MAX_TRIES_BUILD_SOMETHING__ 1440 // IN FRAMES, 1 minute here
#define __PYLON_COVERAGE_TILES__ 4 // length/size of the pylon coverage radius in build tiles
//#define __2_PROBES_PER_ENEMY_WORKER_DEFENSE__ // put 2 probes on one attacking worker in DefendGoal

#ifdef __DEBUG__
//#include <vld.h>
#endif

#ifdef __MICRO_PROJECT__
#undef __ETECH_ESTIMATOR__
#endif

#ifndef __DEBUG__ // Perf
#undef __ARBITRATOR_DEBUG__
//#undef __DO_NOT_HARASS_SCOUT__
///#undef _SECURE_SCL
///#define _SECURE_SCL 0 // remove iterator runtime checks
///#define _HAS_ITERATOR_DEBUGGING 0
#endif