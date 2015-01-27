gulp   = require 'gulp'
shell  = require 'gulp-shell'
server = require 'gulp-server-livereload'
clean  = require 'gulp-clean'

gulp.task 'tex', ->
    gulp.src('tex/*.tex', read: false)
        .pipe(shell [
            'pdflatex -output-directory build <%= file.path %>'
        ])

gulp.task 'server', ->
    gulp.src('build')
        .pipe(server(
            livereload: false
            directoryListing: false
            open: false
            port: 1234
        ))

gulp.task 'clean', ->
    gulp.src('build/*', read: false)
        .pipe(clean())

gulp.task 'watch', ['tex', 'server'], ->
    gulp.watch('tex/*.tex', ['tex'])

gulp.task 'default', ['tex']
