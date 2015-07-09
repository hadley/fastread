#include <Rcpp.h>
using namespace Rcpp;

#include "Source.h"
#include "Tokenizer.h"
#include "TokenizerLine.h"
#include "Collector.h"
#include "CollectorCharacter.h"
#include "Progress.h"
#include "Warnings.h"

// [[Rcpp::export]]
CharacterVector read_file_(List sourceSpec) {
  SourcePtr source = Source::create(sourceSpec);

  CharacterVector out(1);
  out[0] = Rf_mkCharLen(source->begin(), source->end() - source->begin());
  return out;
}

// [[Rcpp::export]]
CharacterVector read_lines_(List sourceSpec, int n_max = -1) {
  SourcePtr source = Source::create(sourceSpec);
  TokenizerLine tokenizer;
  tokenizer.tokenize(source->begin(), source->end());

  int n = (n_max < 0) ? 1000 : n_max;
  CharacterVector out(n);

  int i = 0;
  for (Token t = tokenizer.nextToken(); t.type() != TOKEN_EOF; t = tokenizer.nextToken()) {
    if (i >= n) {
      if (n_max < 0) {
        n = (n * 3)/2 + 1;
        out = Rf_lengthgets(out, n);
      } else {
        break;
      }
    }

    if (t.type() == TOKEN_STRING)
      out[i] = t.asString();

    ++i;
  }

  if (i < n) {
    out = Rf_lengthgets(out, i);
  }

  return out;
}

typedef std::vector<CollectorPtr>::iterator CollectorItr;

// [[Rcpp::export]]
RObject read_tokens(List sourceSpec, List tokenizerSpec, ListOf<List> colSpecs,
                    CharacterVector col_names, int n_max = -1,
                    bool progress = true) {
  Warnings warnings;

  SourcePtr source = Source::create(sourceSpec);

  TokenizerPtr tokenizer = Tokenizer::create(tokenizerSpec);
  tokenizer->tokenize(source->begin(), source->end());
  tokenizer->setWarnings(&warnings);

  std::vector<CollectorPtr> collectors = collectorsCreate(colSpecs, &warnings);

  Progress progressBar;

  size_t p = collectors.size();
  // Work out how many output columns we have
  size_t pOut = 0;
  for(CollectorItr cur = collectors.begin(); cur != collectors.end(); ++cur) {
    if (!(*cur)->skip())
      pOut++;
  }

  // Allow either one name for column, or one name per output col
  if (p != pOut && (size_t) col_names.size() == p) {
    CharacterVector col_names2(pOut);
    int cj = 0;
    for (size_t j = 0; j < p; ++j) {
      if (collectors[j]->skip())
        continue;
      col_names2[cj++] = col_names[j];
    }
    col_names = col_names2;
  }

  if (pOut != (size_t) col_names.size()) {
    Rcpp::stop("You have %i column names, but %i columns",
      col_names.size(), pOut);
  }

  size_t n = (n_max < 0) ? 1000 : n_max;
  collectorsResize(collectors, n);

  size_t i = 0, cells = 0;
  for (Token t = tokenizer->nextToken(); t.type() != TOKEN_EOF; t = tokenizer->nextToken()) {
    if (progress && (cells++) % 250000 == 0)
      progressBar.show(tokenizer->progress());

    if (t.col() >= p) {
      warnings.addWarning(t.row(), t.col(), tfm::format("Only %i columns", p), "");
      continue;
    }

    if (t.row() >= n) {
      if (n_max >= 0)
        break;

      // Estimate rows in full dataset
      n = (i / tokenizer->progress().first) * 1.2;
      collectorsResize(collectors, n);
    }

    collectors[t.col()]->setValue(t.row(), t);
    i = t.row();
  }
  if (progress)
    progressBar.show(tokenizer->progress());
    
  progressBar.stop();

  if (i <= n) {
    collectorsResize(collectors, i + 1);
  }


  // Save individual columns into a data frame
  List out(pOut);
  int j = 0;
  for(CollectorItr cur = collectors.begin(); cur != collectors.end(); ++cur) {
    if ((*cur)->skip())
      continue;

    out[j] = (*cur)->vector();
    j++;
  }

  out.attr("class") = CharacterVector::create("tbl_df", "tbl", "data.frame");
  out.attr("row.names") = IntegerVector::create(NA_INTEGER, -(i + 1));
  out.attr("names") = col_names;

  return warnings.addAsAttribute(out);
}


// [[Rcpp::export]]
std::vector<std::string> collectorsGuess(List sourceSpec, List tokenizerSpec, int n = 100) {
  Warnings warnings;
  SourcePtr source = Source::create(sourceSpec);
  TokenizerPtr tokenizer = Tokenizer::create(tokenizerSpec);
  tokenizer->tokenize(source->begin(), source->end());
  tokenizer->setWarnings(&warnings); // silence warnings

  std::vector<CollectorCharacter> collectors;
  for (Token t = tokenizer->nextToken(); t.type() != TOKEN_EOF; t = tokenizer->nextToken()) {
    if (t.row() >= (size_t) n)
      break;

    // Add new collectors, if needed
    if (t.col() >= collectors.size()) {
      int old_p = collectors.size();
      collectors.resize(t.col() + 1);
      for (size_t j = old_p; j < collectors.size(); ++j) {
        collectors[j].resize(n);
      }
    }

    collectors[t.col()].setValue(t.row(), t);
  }

  std::vector<std::string> out;
  for (size_t j = 0; j < collectors.size(); ++j) {
    CharacterVector col = as<CharacterVector>(collectors[j].vector());
    out.push_back(collectorGuess(col));
  }

  return out;
}


