# readr 0.1.1.9000

* Date time parsing now understands `%p` to parse AM/PM (and am/pm) (#126).

* `read_delim()` now defaults to `escape_backslash = FALSE` and 
  `escape_double = TRUE` for consistency.  
  
* Convenient wrapper around base::saveRDS `write_rds()` for saving a single R 
  object (#140, @nicolasCoutin).  
  
* readr now provides `write_delim()` and `write_tsv()` (#135).

* In `write_csv()`, only use quotes when they're actually needed (#116).
  Write the same amount of precision as used by `as.character()` (#117).
  Format POSIXt vectors in ISO8601 compatible format (#134).
  
* Date time columns are no longer incorrectly read as date columns (#136).

* When guessing field types, and there's no information to go on, use
  character instead of logical (#124, #128).

* Multiple strings can now be specified to indicate missing values (#125), and
  the default for missing strings has been changed to `na = c("", "NA")`.
  Specifying `na = ""` now works as expected with character columns (#114).
  
* `read_*()` methods now throw a warning instead of an error is `col_types`
  specifies type for a non-existant column (#145, @alyst).

* Compound formats %D, %F, %R, %X, %T, %x are now parsed correctly, instead of
  using the ISO8601 parser (#178, @kmillar)

* The completed progress bar is no longer shown after reading a large file if
  `progress = FALSE` (#156, @humburg)
