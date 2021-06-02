#ifndef FASTREAD_SOURCEFILE_H_
#define FASTREAD_SOURCEFILE_H_

#include "Source.h"
#include "cpp11/protect.hpp"

#include "unicode_fopen.h"

class SourceFile : public Source {
  mio::mmap_source source_;
  FILE* fd_;

  const char* begin_;
  const char* end_;

public:
  SourceFile(
      const std::string& path,
      int skip = 0,
      bool skipEmptyRows = true,
      const std::string& comment = "",
      bool skipQuotes = true) {

    std::error_code error;

    fd_ = unicode_fopen(path.c_str(), "rb");

    source_ = mio::make_mmap_source(fileno(fd_), error);

    if (error) {
      cpp11::stop("Cannot read file %s: %s", error.message().c_str());
    }

    begin_ = source_.begin();
    end_ = begin_ + source_.size();

    // Skip byte order mark, if needed
    begin_ = skipBom(begin_, end_);

    // Skip lines, if needed
    begin_ = skipLines(begin_, end_, skip, skipEmptyRows, comment, skipQuotes);
  }
  ~SourceFile() { std::fclose(fd_); }

  const char* begin() { return begin_; }

  const char* end() { return end_; }
};

#endif
