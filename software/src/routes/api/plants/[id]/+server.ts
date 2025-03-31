import prisma from '$lib/prisma';
import type { Plant } from '@prisma/client';
import { json, type RequestHandler } from '@sveltejs/kit';

interface UpdatePlantRequest {
	name: string;
	publishTopic: string;
	subscribeTopic: string;
}

export const GET: RequestHandler = async ({ params }) => {
	try {
		const { id } = params;

		const plant = await prisma.plant.findUnique({
			where: { id }
		});

		if (!plant) {
			return new Response(JSON.stringify({ error: 'Plant not found' }), {
				status: 404,
				headers: { 'Content-Type': 'application/json' }
			});
		}

		return json(plant);
	} catch (error) {
		console.error('Error fetching plant:', error);
		return new Response(JSON.stringify({ error: 'Failed to fetch plant' }), {
			status: 500,
			headers: { 'Content-Type': 'application/json' }
		});
	}
};

export const PUT: RequestHandler = async ({ params, request }) => {
	try {
		const { id } = params;
		const data = (await request.json()) as UpdatePlantRequest;

		if (!data.name || !data.subscribeTopic || !data.publishTopic) {
			return new Response(JSON.stringify({ error: 'Missing required fields' }), {
				status: 400,
				headers: { 'Content-Type': 'application/json' }
			});
		}

		const existingPlant = await prisma.plant.findUnique({
			where: { id }
		});

		if (!existingPlant) {
			return new Response(JSON.stringify({ error: 'Plant not found' }), {
				status: 404,
				headers: { 'Content-Type': 'application/json' }
			});
		}

		const updatedPlant: Plant = await prisma.plant.update({
			where: { id },
			data: {
				name: data.name,
				publishTopic: data.publishTopic,
				subscribeTopic: data.subscribeTopic
			}
		});

		return json(updatedPlant);
	} catch (error) {
		console.error('Error updating plant:', error);
		return new Response(JSON.stringify({ error: 'Failed to update plant' }), {
			status: 500,
			headers: { 'Content-Type': 'application/json' }
		});
	}
};

export const DELETE: RequestHandler = async ({ params }) => {
	try {
		const { id } = params;

		const existingPlant = await prisma.plant.findUnique({
			where: { id }
		});

		if (!existingPlant) {
			return new Response(JSON.stringify({ error: 'Plant not found' }), {
				status: 404,
				headers: { 'Content-Type': 'application/json' }
			});
		}

		await prisma.plant.delete({
			where: { id }
		});

		return new Response(null, { status: 204 });
	} catch (error) {
		console.error('Error deleting plant:', error);
		return new Response(JSON.stringify({ error: 'Failed to delete plant' }), {
			status: 500,
			headers: { 'Content-Type': 'application/json' }
		});
	}
};
