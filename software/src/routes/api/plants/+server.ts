import prisma from '$lib/prisma';
import type { Plant } from '@prisma/client';
import { json, type RequestHandler } from '@sveltejs/kit';

interface CreatePlantRequest {
	name: string;
	publishTopic: string;
	subscribeTopic: string;
}

export const GET: RequestHandler = async () => {
	try {
		const plants: Plant[] = await prisma.plant.findMany({
			orderBy: { createdAt: 'desc' }
		});

		return json(plants);
	} catch (error) {
		console.error('Error fetching plants:', error);
		return new Response(JSON.stringify({ error: 'Failed to fetch plants' }), {
			status: 500,
			headers: { 'Content-Type': 'application/json' }
		});
	}
};

export const POST: RequestHandler = async ({ request }) => {
	try {
		const data = (await request.json()) as CreatePlantRequest;

		if (!data.name || !data.subscribeTopic || !data.publishTopic) {
			return new Response(JSON.stringify({ error: 'Missing required fields' }), {
				status: 400,
				headers: { 'Content-Type': 'application/json' }
			});
		}

		const newPlant = await prisma.plant.create({
			data: {
				name: data.name,
				publishTopic: data.publishTopic,
				subscribeTopic: data.subscribeTopic
			}
		});

		return json(newPlant, { status: 201 });
	} catch (error) {
		console.error('Error creating plant:', error);
		return new Response(JSON.stringify({ error: 'Failed to create plant' }), {
			status: 500,
			headers: { 'Content-Type': 'application/json' }
		});
	}
};
