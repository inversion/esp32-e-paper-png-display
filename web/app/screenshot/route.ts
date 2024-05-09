import fs from 'fs';

export const dynamic = "force-dynamic";

export function GET() {
  const filePath = '/screenshot/screenshot.png';

  if (fs.existsSync(filePath)) {
    const stats = fs.statSync(filePath);
    const file = fs.readFileSync(filePath);
    const response = new Response(file, {
      headers: { 'Content-Type': 'image/png', 'Content-Length': `${stats.size}`},
      status: 200
    });
    return response;
  } else {
    const response = new Response('File not found', { status: 404 });
    return response;
  }
};
