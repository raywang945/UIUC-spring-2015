gulp   = require 'gulp'
sass   = require 'gulp-sass'
coffee = require 'gulp-coffee'
jade   = require 'gulp-jade'
server = require 'gulp-server-livereload'

gulp.task 'jade', ->
    gulp.src('src/jade/*.jade')
        .pipe(jade())
        .pipe(gulp.dest('.'))

gulp.task 'coffee', ->
    gulp.src('src/coffee/*.coffee')
        .pipe(coffee(bare: true))
        .pipe(gulp.dest('js'))

gulp.task 'sass', ->
    gulp.src('src/sass/*.sass')
        .pipe(sass(indentedSyntax: true))
        .pipe(gulp.dest('css'))

gulp.task 'watch', ['jade', 'coffee', 'sass'], ->
    gulp.watch('src/jade/*.jade', ['jade'])
    gulp.watch('src/coffee/*.coffee', ['coffee'])
    gulp.watch('src/sass/*.sass', ['sass'])

gulp.task 'server', ['watch'], ->
    gulp.src('.')
        .pipe(server(
            livereload: true,
            open: true,
            defaultFile: 'index.html',
            port: 9000
        ))

gulp.task 'default', ['jade', 'coffee', 'sass']
