#include <fstream>

#include "model.hh"

void ProbabilityTables::serialize( std::ofstream & output ) const
{
  output.write( reinterpret_cast<char*>( model_.get() ), sizeof( *model_ ) );
}

void ProbabilityTables::optimize()
{
  model_->forall( [&] ( Branch & x ) { x.optimize(); } );
}
