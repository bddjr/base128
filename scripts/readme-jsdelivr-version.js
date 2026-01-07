import fs from 'fs'
import package_json from '../package.json' with { type: 'json' }

const readmeName = 'README.md'

const oldReadme = fs.readFileSync(readmeName).toString()

let newReadme = oldReadme.replaceAll(/(?<=cdn\.jsdelivr\.net\/npm\/base128-ascii@)([\d\.]*|latest)/g, package_json.version)

newReadme !== oldReadme && fs.writeFileSync(readmeName, newReadme)
