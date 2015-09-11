#ifndef FASTREAD_DATE_TIME_PARSER_H_
#define FASTREAD_DATE_TIME_PARSER_H_

#include <ctime>
#include "DateTime.h"
#include "LocaleInfo.h"
#include <boost/algorithm/string/predicate.hpp>
#include "QiParsers.h"

// Parsing ---------------------------------------------------------------------

class DateTimeParser {
  int year_, mon_, day_, hour_, min_, sec_;
  double psec_;
  int amPm_;

  int tzOffsetHours_, tzOffsetMinutes_;
  std::string tz_;

  LocaleInfo* pLocale_;
  std::string tzDefault_;

  const char* dateItr_;
  const char* dateEnd_;

public:
  DateTimeParser(LocaleInfo* pLocale):
  pLocale_(pLocale), tzDefault_(pLocale->tz_)
  {
  }

  // Parse ISO8601 date time. In benchmarks this only seems ~30% faster than
  // parsing with a format string so it doesn't seem necessary to add individual
  // parsers for other common formats.
  bool parseISO8601(bool partial = true) {
    // Date: YYYY-MM-DD, YYYYMMDD
    if (!consumeInteger(4, &year_))
      return false;
    consumeThisChar('-');
    if (!consumeInteger1(2, &mon_))
      return false;
    consumeThisChar('-');
    if (!consumeInteger1(2, &day_))
      return false;

    if (isComplete())
      return true;

    // Technically, spec requires T, but very common to use
    char next;
    if (!consumeChar(&next))
      return false;
    if (next != 'T' && next != ' ')
      return false;

    // hh:mm:ss.sss, hh:mm:ss, hh:mm, hh
    // hhmmss.sss, hhmmss, hhmm
    if (!consumeInteger(2, &hour_))
      return false;
    consumeThisChar(':');
    consumeInteger(2, &min_);
    consumeThisChar(':');
    consumeSeconds(&sec_, &psec_);

    if (isComplete())
      return true;

    // Has a timezone
    tz_ = "UTC";
    if (!consumeTzOffset(&tzOffsetHours_, &tzOffsetMinutes_))
      return false;

    return isComplete();
  }

  bool isComplete() {
    return dateItr_ == dateEnd_;
  }

  void setDate(const char* date) {
    init(date);
    reset();
  }

  bool parse(const std::string& format) {
    consumeWhiteSpace(); // always consume leading whitespace

    std::string::const_iterator formatItr, formatEnd = format.end();
    for (formatItr = format.begin(); formatItr != formatEnd; ++formatItr) {
      // Whitespace in format matches 0 or more whitespace in date
      if (std::isspace(*formatItr)) {
        consumeWhiteSpace();
        continue;
      }

      // Any other characters much much exactly.
      if (*formatItr != '%') {
        if (!consumeThisChar(*formatItr))
          return false;
        continue;
      }

      if (formatItr + 1 == formatEnd)
        Rcpp::stop("Invalid format: trailing %");
      formatItr++;

      switch(*formatItr) {
      case 'Y': // year with century
        if (!consumeInteger(4, &year_))
          return false;
        break;
      case 'y': // year without century
        if (!consumeInteger(2, &year_))
          return false;
        year_ += (year_ < 69) ? 2000 : 1900;
        break;
      case 'm': // month
        if (!consumeInteger1(2, &mon_))
          return false;
        break;
      case 'b': // abbreviated month name
        if (!consumeString(pLocale_->monAb_, &mon_))
          return false;
        break;
      case 'B': // month name
        if (!consumeString(pLocale_->mon_, &mon_))
          return false;
        break;
      case 'd': // day
        if (!consumeInteger1(2, &day_))
          return false;
        break;
      case 'e': // day with optional leading space
        if (!consumeInteger1WithSpace(2, &day_))
          return false;
        break;
      case 'H': // hour
        if (!consumeInteger(2, &hour_))
          return false;
        break;
      case 'M': // minute
        if (!consumeInteger(2, &min_))
          return false;
        break;
      case 'S': // seconds (integer)
        if (!consumeSeconds(&sec_, NULL))
          return false;
        break;
      case 'O': // seconds (double)
        if (formatItr + 1 == formatEnd || *(formatItr + 1) != 'S')
          Rcpp::stop("Invalid format: %%O must be followed by %%S");
        formatItr++;
        if (!consumeSeconds(&sec_, &psec_))
          return false;
        break;

      case 'p': // AM/PM
        if (!consumeString(pLocale_->amPm_, &amPm_))
          return false;
        break;

      case 'z': // time zone specification
        tz_ = "UTC";
        if (!consumeTzOffset(&tzOffsetHours_, &tzOffsetMinutes_))
          return false;
        break;
      case 'Z': // time zone name
        if (!consumeTzName(&tz_))
          return false;
        break;

        // Extensions
      case '.':
        consumeNonDigit();
        break;

      case '*':
        consumeNonDigits();
        break;

        // Compound formats
      case 'D':
        parse("%m/%d/%y");
        break;
      case 'F':
        parse("%Y-%m-%d");
        break;
      case 'R':
        parse("%H:%M");
        break;
      case 'X':
      case 'T':
        parse("%H:%M:%S");
        break;
      case 'x':
        parse("%y/%m/%d");
        break;

      default:
        Rcpp::stop("Unsupported format %%%s", *formatItr);
      }
    }

    consumeWhiteSpace(); // always consume trailing whitespace

    return isComplete();
  }

  DateTime makeDateTime() {
    DateTime dt(year_, mon_, day_, hour(), min_, sec_, psec_, tz_);
    if (tz_ == "UTC")
      dt.setOffset(-tzOffsetHours_ * 3600 - tzOffsetMinutes_ * 60);

    return dt;
  }
  DateTime makeDate() {
    DateTime dt(year_, mon_, day_, 0, 0, 0, 0, "UTC");
    return dt;
  }
  DateTime makeTime() {
    DateTime dt(0, 0, 0, hour(), min_, sec_, psec_, "UTC");
    return dt;
  }

private:
  int hour() {
    return hour_ + (amPm_ == 1 ? 12 : 0);
  }

  inline bool consumeSeconds(int* pSec, double* pPartialSec) {
    double sec;
    if (!consumeDouble(&sec))
      return false;

    *pSec = (int) sec;
    if (pPartialSec != NULL)
      *pPartialSec = sec - *pSec;
    return true;
  }

  inline bool consumeString(const std::vector<std::string>& haystack, int* pOut) {
    boost::iterator_range<const char*> needle(dateItr_, dateEnd_);

    for(size_t i = 0; i < haystack.size(); ++i) {
      if (boost::istarts_with(needle, haystack[i])) {
        *pOut = i;
        dateItr_ += haystack[i].size();
        return true;
      }
    }

    return false;
  }

  inline bool consumeInteger(int n, int* pOut) {
    if (dateItr_ == dateEnd_ || *dateItr_ == '-' || *dateItr_ == '+')
      return false;

    const char* end = std::min(dateItr_ + n, dateEnd_);
    return parseInt(dateItr_, end, *pOut);
  }

  // Integer indexed from 1 (i.e. month and date)
  inline bool consumeInteger1(int n, int* pOut) {
    if (!consumeInteger(n, pOut))
      return false;

    (*pOut)--;
    return true;
  }

  // Integer indexed from 1 with optional space
  inline bool consumeInteger1WithSpace(int n, int* pOut) {
    if (consumeThisChar(' '))
      n--;

    return consumeInteger1(n, pOut);
  }

  inline bool consumeDouble(double* pOut) {
    if (dateItr_ == dateEnd_ || *dateItr_ == '-' || *dateItr_ == '+')
      return false;
    return parseDouble(pLocale_->decimalMark_, dateItr_, dateEnd_, *pOut);
  }

  inline bool consumeWhiteSpace() {
    while (dateItr_ != dateEnd_ && std::isspace(*dateItr_))
      dateItr_++;

    return true;
  }

  inline bool consumeNonDigit() {
    if (dateItr_ == dateEnd_ || std::isdigit(*dateItr_))
      return false;

    dateItr_++;
    return true;
  }

  inline bool consumeNonDigits() {
    while (dateItr_ != dateEnd_ && !std::isdigit(*dateItr_))
      dateItr_++;

    return true;
  }

  inline bool consumeChar(char* pOut) {
    if (dateItr_ == dateEnd_)
      return false;

    *pOut = *dateItr_++;
    return true;
  }

  inline bool consumeThisChar(char needed) {
    if (dateItr_ == dateEnd_ || *dateItr_ != needed)
      return false;

    dateItr_++;
    return true;
  }

  inline bool consumeAMPM(bool* pIsPM) {
    if (dateItr_ == dateEnd_)
      return false;

    if (consumeThisChar('A') || consumeThisChar('a')) {
      *pIsPM = false;
    } else if (consumeThisChar('P') || consumeThisChar('p')) {
      *pIsPM = true;
    } else {
      return false;
    }

    if (!(consumeThisChar('M') || consumeThisChar('m')))
      return false;

    return true;
  }

  // ISO8601 style
  // Z
  // ±hh:mm
  // ±hhmm
  // ±hh
  inline bool consumeTzOffset(int* pHours, int* pMinutes) {
    if (consumeThisChar('Z'))
      return true;

    // Optional +/- (required for ISO8601 but we'll let it slide)
    int mult = 1;
    if (*dateItr_ == '+' || *dateItr_ == '-') {
      mult = (*dateItr_ == '-') ? -1 : 1;
      dateItr_++;
    }

    // Required hours
    if (!consumeInteger(2, pHours))
      return false;

    // Optional colon and minutes
    consumeThisChar(':');
    consumeInteger(2, pMinutes);

    *pHours *= mult;
    *pMinutes *= mult;

    return true;
  }

  inline bool consumeTzName(std::string* pOut) {
    const char* tzStart = dateItr_;
    while (dateItr_ != dateEnd_ && !std::isspace(*dateItr_))
      dateItr_++;

    pOut->assign(tzStart, dateItr_);
    return tzStart != dateItr_;
  }


  void init(const char* date) {
    reset();
    dateItr_ = date;

    // find terminating null character
    dateEnd_ = date;
    while (*dateEnd_)
      dateEnd_++;
  }

  void reset() {
    year_ = -1;
    mon_ = -1;
    day_ = -1;
    hour_ = 0;
    min_ = 0;
    sec_ = 0;
    psec_ = 0;
    amPm_ = 0;

    tzOffsetHours_ = 0;
    tzOffsetMinutes_ = 0;
    tz_ = tzDefault_;
  }
};

#endif
