
#include <gsCore/gsTemplateTools.h>

#include <gsElasticity/gsElasticityFunctions.h>
#include <gsElasticity/gsElasticityFunctions.hpp>

#include <gsElasticity/gsElasticityAssembler.h>
#include <gsElasticity/gsElasticityAssembler.hpp>

//#include <gsElasticity/gsElasticityMassAssembler.h>
//#include <gsElasticity/gsElasticityMassAssembler.hpp>

//#include <gsElasticity/gsElasticityMixedTHAssembler.h>
//#include <gsElasticity/gsElasticityMixedTHAssembler.hpp>

//#include <gsElasticity/gsElasticityNewton.h>

//#include <gsElasticity/gsElasticityMixedTHNewton.h>

namespace gismo
{

    CLASS_TEMPLATE_INST gsCauchyStressFunction<real_t>;
    CLASS_TEMPLATE_INST gsElasticityAssembler<real_t>;
//	CLASS_TEMPLATE_INST gsElasticityMassAssembler<real_t>;
//	CLASS_TEMPLATE_INST gsElasticityMixedTHAssembler<real_t>;
//	CLASS_TEMPLATE_INST gsElasticityNewton<real_t>;
//	CLASS_TEMPLATE_INST gsElasticityMixedTHNewton<real_t>;
}
