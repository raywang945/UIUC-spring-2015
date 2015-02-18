gulp   = require 'gulp'
shell  = require 'gulp-shell'
clean  = require 'gulp-clean'

gulp.task 'tex', ->
    gulp.src('tex/*.tex', read: false)
        .pipe(shell [
            'pdflatex -output-directory build <%= file.path %>'
        ])

gulp.task 'clean', ->
    gulp.src('build/*', read: false)
        .pipe(clean())

gulp.task 'watch', ['tex'], ->
    gulp.watch('tex/*.tex', ['tex'])

gulp.task 'default', ['tex']
